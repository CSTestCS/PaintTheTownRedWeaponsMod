// FBX reading: binary (6.1 - 7.7) and ASCII (7.x) files into a node tree, then a single combined
// static mesh in Unity's coordinate system (left-handed, Y up, metres).
#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>
#include "math.hpp"

namespace cw {

using FbxProp = std::variant<int64_t, double, bool, std::string, std::vector<uint8_t>,
                             std::vector<double>, std::vector<int64_t>>;

struct FbxNode {
    std::string name;
    std::vector<FbxProp> props;
    std::vector<std::unique_ptr<FbxNode>> children;

    const FbxNode* child(const std::string& n) const;
    std::vector<const FbxNode*> childrenNamed(const std::string& n) const;
    std::string propString(size_t i) const;
    int64_t propLong(size_t i) const;
    double propDouble(size_t i) const;
    std::vector<double> asDoubles() const;
    std::vector<int64_t> asLongs() const;
    const std::vector<uint8_t>* propBytes(size_t i) const;
    /// Properties70 block as name -> P node
    std::map<std::string, const FbxNode*> properties70() const;
};

struct FbxDocument {
    FbxNode root;
    int version = 0;
    bool binary = false;
};

/// Throws std::runtime_error on malformed files.
FbxDocument fbxParse(const std::vector<uint8_t>& data);

struct MeshData {
    std::vector<Vec3> positions, normals;
    std::vector<Vec2> uvs;
    std::vector<Vec4> colors;
    std::vector<int> indices;
    bool hasNormals = false, hasUVs = false, hasColors = false;
    std::vector<uint8_t> embeddedTexture;
    std::vector<std::string> warnings;

    size_t triangleCount() const { return indices.size() / 3; }
    void bounds(Vec3& mn, Vec3& mx) const;
};

MeshData fbxExtractMesh(const FbxDocument& doc);
MeshData fbxLoadMesh(const std::string& path);

std::vector<uint8_t> readFileBytes(const std::string& path);
std::string fbxCleanName(const std::string& raw);

} // namespace cw
