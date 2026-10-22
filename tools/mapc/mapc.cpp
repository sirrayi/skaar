// mapc: compile a JSON list of axis-aligned boxes into a Quake 3 BSP (IBSP v46).
//
//   mapc <input.json> <output.bsp>
//
// The output is deliberately simple: one BSP node whose children both point at
// a single leaf holding every brush and surface, one vis cluster that sees
// itself (everything is visible), and vertex lighting baked from an ambient term plus one sun.
// Collision tests every brush per trace, which is fine for test rooms and
// aim-trainer arenas with tens of brushes. See docs/mapc.md for the format.
//
// This file is part of Skaar. GPLv2 or later.

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "json.hpp"

using json = nlohmann::json;

namespace {

// ---- on-disk structures (mirror code/qcommon/qfiles.h) ---------------------

constexpr int kBspVersion = 46;
constexpr int kNumLumps = 17;
constexpr int kContentsSolid = 1;
constexpr int kSurfacePlanar = 1;  // MST_PLANAR
constexpr int kLightmapByVertex = -3;  // LIGHTMAP_BY_VERTEX (-1 would mean unlit-by-map, lit like a model)

// The renderer doubles map lighting (r_mapOverBrightBits 2 minus
// r_overBrightBits 1), so light values are stored at half intensity, as q3map2 does
constexpr float kOverbrightScale = 0.5f;

// renderergl2 defaults; a worldspawn "gridsize" key would override them
constexpr float kLightGridSize[3] = {64.0f, 64.0f, 128.0f};

enum Lump {
    kEntities, kShaders, kPlanes, kNodes, kLeafs, kLeafSurfaces, kLeafBrushes, kModels,
    kBrushes, kBrushSides, kDrawVerts, kDrawIndexes, kFogs, kSurfaces, kLightmaps,
    kLightGrid, kVisibility
};

struct LumpEntry { int32_t offset, length; };
struct Header { char ident[4]; int32_t version; LumpEntry lumps[kNumLumps]; };
struct DShader { char name[64]; int32_t surfaceFlags, contentFlags; };
struct DPlane { float normal[3]; float dist; };
struct DNode { int32_t planeNum; int32_t children[2]; int32_t mins[3], maxs[3]; };
struct DLeaf {
    int32_t cluster, area;
    int32_t mins[3], maxs[3];
    int32_t firstLeafSurface, numLeafSurfaces;
    int32_t firstLeafBrush, numLeafBrushes;
};
struct DModel { float mins[3], maxs[3]; int32_t firstSurface, numSurfaces, firstBrush, numBrushes; };
struct DBrushSide { int32_t planeNum, shaderNum; };
struct DBrush { int32_t firstSide, numSides, shaderNum; };
struct DrawVert { float xyz[3]; float st[2]; float lightmap[2]; float normal[3]; uint8_t color[4]; };
struct DSurface {
    int32_t shaderNum, fogNum, surfaceType;
    int32_t firstVert, numVerts, firstIndex, numIndexes;
    int32_t lightmapNum, lightmapX, lightmapY, lightmapWidth, lightmapHeight;
    float lightmapOrigin[3];
    float lightmapVecs[3][3];
    int32_t patchWidth, patchHeight;
};

static_assert(sizeof(Header) == 144);
static_assert(sizeof(DShader) == 72);
static_assert(sizeof(DPlane) == 16);
static_assert(sizeof(DNode) == 36);
static_assert(sizeof(DLeaf) == 48);
static_assert(sizeof(DModel) == 40);
static_assert(sizeof(DBrushSide) == 8);
static_assert(sizeof(DBrush) == 12);
static_assert(sizeof(DrawVert) == 44);
static_assert(sizeof(DSurface) == 104);

// ---- input -----------------------------------------------------------------

using Vec3 = std::array<float, 3>;

struct Box {
    Vec3 min, max;
    std::string material;
};

struct Light {
    float ambient = 0.45f;
    Vec3 sunDir = {0.35f, 0.25f, -1.0f};  // direction the light travels
    float sun = 0.55f;
};

Vec3 ReadVec3(const json& j, const char* what) {
    if (!j.is_array() || j.size() != 3) {
        throw std::runtime_error(std::string(what) + " must be an array of 3 numbers");
    }
    return {j[0].get<float>(), j[1].get<float>(), j[2].get<float>()};
}

std::string FormatVec3(const Vec3& v) {
    char buf[96];
    std::snprintf(buf, sizeof(buf), "%g %g %g", v[0], v[1], v[2]);
    return buf;
}

// "room": hollow box built from six wall brushes around an interior volume
void AddRoom(const json& room, std::vector<Box>& boxes) {
    const Vec3 mn = ReadVec3(room.at("min"), "room.min");
    const Vec3 mx = ReadVec3(room.at("max"), "room.max");
    const float t = room.value("wall", 16.0f);
    const std::string walls = room.value("walls", std::string("wall"));
    const std::string floorMat = room.value("floor", walls);
    const std::string ceiling = room.value("ceiling", walls);

    boxes.push_back({{mn[0] - t, mn[1] - t, mn[2] - t}, {mx[0] + t, mx[1] + t, mn[2]}, floorMat});
    boxes.push_back({{mn[0] - t, mn[1] - t, mx[2]}, {mx[0] + t, mx[1] + t, mx[2] + t}, ceiling});
    boxes.push_back({{mn[0] - t, mn[1] - t, mn[2]}, {mn[0], mx[1] + t, mx[2]}, walls});
    boxes.push_back({{mx[0], mn[1] - t, mn[2]}, {mx[0] + t, mx[1] + t, mx[2]}, walls});
    boxes.push_back({{mn[0], mn[1] - t, mn[2]}, {mx[0], mn[1], mx[2]}, walls});
    boxes.push_back({{mn[0], mx[1], mn[2]}, {mx[0], mx[1] + t, mx[2]}, walls});
}

// ---- compiler --------------------------------------------------------------

class Compiler {
public:
    void Load(const json& doc) {
        message_ = doc.value("message", doc.value("name", std::string("untitled")));

        if (doc.contains("materials")) {
            for (const auto& [key, value] : doc["materials"].items()) {
                materials_[key] = value.get<std::string>();
            }
        }
        if (doc.contains("light")) {
            const json& l = doc["light"];
            light_.ambient = l.value("ambient", light_.ambient);
            light_.sun = l.value("sun", light_.sun);
            if (l.contains("sunDir")) {
                light_.sunDir = ReadVec3(l["sunDir"], "light.sunDir");
            }
        }
        if (doc.contains("rooms")) {
            for (const json& room : doc["rooms"]) {
                AddRoom(room, boxes_);
            }
        }
        if (doc.contains("brushes")) {
            for (const json& b : doc["brushes"]) {
                Box box{ReadVec3(b.at("min"), "brush.min"), ReadVec3(b.at("max"), "brush.max"),
                        b.value("material", std::string("wall"))};
                for (int i = 0; i < 3; i++) {
                    if (box.min[i] >= box.max[i]) {
                        throw std::runtime_error("brush min must be < max on every axis");
                    }
                }
                boxes_.push_back(box);
            }
        }
        if (boxes_.empty()) {
            throw std::runtime_error("map has no brushes or rooms");
        }
        if (doc.contains("entities")) {
            entities_ = doc["entities"];
        }
    }

    std::vector<uint8_t> Compile() {
        BuildBrushesAndSurfaces();
        BuildTree();
        return Write();
    }

    size_t NumBrushes() const { return brushes_.size(); }
    size_t NumSurfaces() const { return surfaces_.size(); }

private:
    int ShaderFor(const std::string& material) {
        auto it = materials_.find(material);
        const std::string name = it != materials_.end() ? it->second : "textures/skaar/" + material;
        for (size_t i = 0; i < shaders_.size(); i++) {
            if (name == shaders_[i].name) {
                return static_cast<int>(i);
            }
        }
        if (name.size() >= sizeof(DShader::name)) {
            throw std::runtime_error("material name too long: " + name);
        }
        DShader s{};
        std::memcpy(s.name, name.c_str(), name.size());
        s.contentFlags = kContentsSolid;
        shaders_.push_back(s);
        return static_cast<int>(shaders_.size() - 1);
    }

    uint8_t Shade(const float n[3]) const {
        Vec3 d = light_.sunDir;
        const float len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        const float lambert = std::fmax(0.0f, -(n[0] * d[0] + n[1] * d[1] + n[2] * d[2]) / len);
        const float v = std::fmin(1.0f, light_.ambient + light_.sun * lambert);
        return static_cast<uint8_t>(std::lround(v * 255.0f * kOverbrightScale));
    }

    void BuildBrushesAndSurfaces() {
        for (const Box& box : boxes_) {
            const int shader = ShaderFor(box.material);
            DBrush brush{static_cast<int32_t>(sides_.size()), 6, shader};

            // q3 convention: the first six sides are the axial planes in the
            // order -x, +x, -y, +y, -z, +z (CM_BoundBrush relies on it)
            for (int axis = 0; axis < 3; axis++) {
                for (int positive = 0; positive < 2; positive++) {
                    DPlane plane{};
                    plane.normal[axis] = positive ? 1.0f : -1.0f;
                    plane.dist = positive ? box.max[axis] : -box.min[axis];
                    sides_.push_back({static_cast<int32_t>(planes_.size()), shader});
                    planes_.push_back(plane);
                    AddFace(box, axis, positive != 0, shader);
                }
            }
            brushes_.push_back(brush);
            for (int i = 0; i < 3; i++) {
                mins_[i] = std::fmin(mins_[i], box.min[i]);
                maxs_[i] = std::fmax(maxs_[i], box.max[i]);
            }
        }
    }

    // One quad per brush side, facing out of the brush
    void AddFace(const Box& box, int axis, bool positive, int shader) {
        const int u = (axis + 1) % 3, v = (axis + 2) % 3;
        const float w = positive ? box.max[axis] : box.min[axis];
        float normal[3] = {0, 0, 0};
        normal[axis] = positive ? 1.0f : -1.0f;

        DSurface surf{};
        surf.shaderNum = shader;
        surf.fogNum = -1;
        surf.surfaceType = kSurfacePlanar;
        surf.firstVert = static_cast<int32_t>(verts_.size());
        surf.numVerts = 4;
        surf.firstIndex = static_cast<int32_t>(indexes_.size());
        surf.numIndexes = 6;
        surf.lightmapNum = kLightmapByVertex;
        std::memcpy(surf.lightmapVecs[2], normal, sizeof(normal));

        const uint8_t shade = Shade(normal);
        const float corners[4][2] = {
            {box.min[u], box.min[v]}, {box.max[u], box.min[v]},
            {box.max[u], box.max[v]}, {box.min[u], box.max[v]},
        };
        for (const auto& c : corners) {
            DrawVert vert{};
            vert.xyz[axis] = w;
            vert.xyz[u] = c[0];
            vert.xyz[v] = c[1];
            // world-aligned texture mapping: one texture repeat per 128 units
            vert.st[0] = c[0] / 128.0f;
            vert.st[1] = -c[1] / 128.0f;
            std::memcpy(vert.normal, normal, sizeof(normal));
            vert.color[0] = vert.color[1] = vert.color[2] = shade;
            vert.color[3] = 255;
            verts_.push_back(vert);
        }

        // q3 renders clockwise-from-the-front triangles; (u, v, axis) is
        // right-handed, so corners run counter-clockwise seen from +axis
        static const int kCcw[6] = {0, 1, 2, 0, 2, 3};
        static const int kCw[6] = {0, 2, 1, 0, 3, 2};
        const int* order = positive ? kCw : kCcw;
        for (int i = 0; i < 6; i++) {
            indexes_.push_back(order[i]);
        }
        surfaces_.push_back(surf);
    }

    void BuildTree() {
        // Root node: any plane will do, both children are the single leaf
        DNode node{};
        node.planeNum = 0;
        node.children[0] = node.children[1] = -1;  // -1 - leaf 0

        DLeaf leaf{};
        leaf.cluster = 0;
        leaf.area = 0;
        leaf.firstLeafSurface = 0;
        leaf.numLeafSurfaces = static_cast<int32_t>(surfaces_.size());
        leaf.firstLeafBrush = 0;
        leaf.numLeafBrushes = static_cast<int32_t>(brushes_.size());

        for (int i = 0; i < 3; i++) {
            node.mins[i] = leaf.mins[i] = static_cast<int32_t>(std::floor(mins_[i])) - 1;
            node.maxs[i] = leaf.maxs[i] = static_cast<int32_t>(std::ceil(maxs_[i])) + 1;
        }
        nodes_.push_back(node);
        leafs_.push_back(leaf);

        for (size_t i = 0; i < surfaces_.size(); i++) {
            leafSurfaces_.push_back(static_cast<int32_t>(i));
        }
        for (size_t i = 0; i < brushes_.size(); i++) {
            leafBrushes_.push_back(static_cast<int32_t>(i));
        }

        DModel world{};
        for (int i = 0; i < 3; i++) {
            world.mins[i] = mins_[i];
            world.maxs[i] = maxs_[i];
        }
        world.numSurfaces = static_cast<int32_t>(surfaces_.size());
        world.numBrushes = static_cast<int32_t>(brushes_.size());
        models_.push_back(world);

        BuildLightGrid();
    }

    // Must match R_LoadLightGrid's sizing exactly, or the renderer rejects it
    void BuildLightGrid() {
        int bounds[3];
        for (int i = 0; i < 3; i++) {
            const float origin = kLightGridSize[i] * std::ceil(mins_[i] / kLightGridSize[i]);
            const float max = kLightGridSize[i] * std::floor(maxs_[i] / kLightGridSize[i]);
            bounds[i] = static_cast<int>((max - origin) / kLightGridSize[i]) + 1;
        }

        // direction towards the light, encoded as the renderer decodes it:
        // x = cos(lat) sin(lng), y = sin(lat) sin(lng), z = cos(lng)
        Vec3 d = light_.sunDir;
        const float len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        const float tx = -d[0] / len, ty = -d[1] / len, tz = -d[2] / len;
        const float twoPi = 6.28318530718f;
        float lat = std::atan2(ty, tx);
        if (lat < 0) {
            lat += twoPi;
        }
        const float lng = std::acos(std::fmax(-1.0f, std::fmin(1.0f, tz)));

        const auto byte = [](float v) {
            return static_cast<uint8_t>(std::lround(std::fmin(1.0f, v) * 255.0f * kOverbrightScale));
        };
        const uint8_t point[8] = {
            byte(light_.ambient), byte(light_.ambient), byte(light_.ambient),
            byte(light_.sun), byte(light_.sun), byte(light_.sun),
            static_cast<uint8_t>(std::lround(lng / twoPi * 256.0f) & 255),
            static_cast<uint8_t>(std::lround(lat / twoPi * 256.0f) & 255),
        };

        const size_t numPoints = static_cast<size_t>(bounds[0]) * bounds[1] * bounds[2];
        lightGrid_.reserve(numPoints * 8);
        for (size_t i = 0; i < numPoints; i++) {
            lightGrid_.insert(lightGrid_.end(), point, point + 8);
        }
    }

    std::string EntityString() const {
        std::string out = "{\n\"classname\" \"worldspawn\"\n\"message\" \"" + message_ + "\"\n}\n";
        for (const json& ent : entities_) {
            out += "{\n";
            for (const auto& [key, value] : ent.items()) {
                std::string text;
                if (value.is_string()) {
                    text = value.get<std::string>();
                } else if (value.is_array()) {
                    text = FormatVec3(ReadVec3(value, key.c_str()));
                } else {
                    text = value.dump();
                }
                out += "\"" + key + "\" \"" + text + "\"\n";
            }
            out += "}\n";
        }
        return out;
    }

    template <typename T>
    static void Append(std::vector<uint8_t>& out, Header& header, Lump lump, const T* data, size_t count) {
        while (out.size() % 4) {
            out.push_back(0);
        }
        header.lumps[lump].offset = static_cast<int32_t>(out.size());
        header.lumps[lump].length = static_cast<int32_t>(count * sizeof(T));
        const auto* bytes = reinterpret_cast<const uint8_t*>(data);
        out.insert(out.end(), bytes, bytes + count * sizeof(T));
    }

    std::vector<uint8_t> Write() const {
        std::vector<uint8_t> out(sizeof(Header), 0);
        Header header{};
        std::memcpy(header.ident, "IBSP", 4);
        header.version = kBspVersion;

        const std::string ents = EntityString();
        Append(out, header, kEntities, ents.c_str(), ents.size() + 1);
        Append(out, header, kShaders, shaders_.data(), shaders_.size());
        Append(out, header, kPlanes, planes_.data(), planes_.size());
        Append(out, header, kNodes, nodes_.data(), nodes_.size());
        Append(out, header, kLeafs, leafs_.data(), leafs_.size());
        Append(out, header, kLeafSurfaces, leafSurfaces_.data(), leafSurfaces_.size());
        Append(out, header, kLeafBrushes, leafBrushes_.data(), leafBrushes_.size());
        Append(out, header, kModels, models_.data(), models_.size());
        Append(out, header, kBrushes, brushes_.data(), brushes_.size());
        Append(out, header, kBrushSides, sides_.data(), sides_.size());
        Append(out, header, kDrawVerts, verts_.data(), verts_.size());
        Append(out, header, kDrawIndexes, indexes_.data(), indexes_.size());
        Append<uint8_t>(out, header, kFogs, nullptr, 0);
        Append(out, header, kSurfaces, surfaces_.data(), surfaces_.size());
        Append<uint8_t>(out, header, kLightmaps, nullptr, 0);
        Append(out, header, kLightGrid, lightGrid_.data(), lightGrid_.size());
        // The renderer skips leafs whose cluster >= numClusters, so even a
        // one-leaf map needs a vis lump: numClusters, clusterBytes, then one
        // row per cluster (cluster 0 sees itself)
        const int32_t vis[3] = {1, 4, 1};
        Append(out, header, kVisibility, vis, 3);

        std::memcpy(out.data(), &header, sizeof(header));
        return out;
    }

    std::string message_;
    std::map<std::string, std::string> materials_;
    Light light_;
    std::vector<Box> boxes_;
    json entities_ = json::array();

    Vec3 mins_ = {1e9f, 1e9f, 1e9f};
    Vec3 maxs_ = {-1e9f, -1e9f, -1e9f};

    std::vector<DShader> shaders_;
    std::vector<DPlane> planes_;
    std::vector<DNode> nodes_;
    std::vector<DLeaf> leafs_;
    std::vector<int32_t> leafSurfaces_;
    std::vector<int32_t> leafBrushes_;
    std::vector<DModel> models_;
    std::vector<DBrush> brushes_;
    std::vector<DBrushSide> sides_;
    std::vector<DrawVert> verts_;
    std::vector<int32_t> indexes_;
    std::vector<DSurface> surfaces_;
    std::vector<uint8_t> lightGrid_;
};

}  // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: mapc <input.json> <output.bsp>\n");
        return 2;
    }

    try {
        std::ifstream in(argv[1]);
        if (!in) {
            throw std::runtime_error("can't open input");
        }
        const json doc = json::parse(in, nullptr, true, true);  // allow comments

        Compiler compiler;
        compiler.Load(doc);
        const std::vector<uint8_t> bsp = compiler.Compile();

        std::ofstream out(argv[2], std::ios::binary);
        if (!out) {
            throw std::runtime_error(std::string("can't write ") + argv[2]);
        }
        out.write(reinterpret_cast<const char*>(bsp.data()), static_cast<std::streamsize>(bsp.size()));
        std::printf("mapc: %s -> %s (%zu brushes, %zu surfaces, %zu bytes)\n", argv[1], argv[2],
                    compiler.NumBrushes(), compiler.NumSurfaces(), bsp.size());
    } catch (const std::exception& e) {
        // file:line style so the dev-watch output and toasts point at the source
        std::fprintf(stderr, "%s:1: error: mapc: %s\n", argv[1], e.what());
        return 1;
    }
    return 0;
}
