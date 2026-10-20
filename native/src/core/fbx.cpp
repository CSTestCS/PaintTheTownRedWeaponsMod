#include "fbx.hpp"
#include <cstring>
#include <fstream>
#include <functional>
#include <stdexcept>
#include "../../third_party/stb/stb_image.h"

namespace cw {

// ------------------------------------------------------------------ node helpers

const FbxNode* FbxNode::child(const std::string& n) const {
    for (auto& c : children) if (c->name == n) return c.get();
    return nullptr;
}

std::vector<const FbxNode*> FbxNode::childrenNamed(const std::string& n) const {
    std::vector<const FbxNode*> r;
    for (auto& c : children) if (c->name == n) r.push_back(c.get());
    return r;
}

std::string FbxNode::propString(size_t i) const {
    if (i >= props.size()) return "";
    const auto& p = props[i];
    if (auto s = std::get_if<std::string>(&p)) return *s;
    if (auto l = std::get_if<int64_t>(&p)) return std::to_string(*l);
    if (auto d = std::get_if<double>(&p)) return std::to_string(*d);
    if (auto b = std::get_if<bool>(&p)) return *b ? "1" : "0";
    return "";
}

int64_t FbxNode::propLong(size_t i) const {
    if (i >= props.size()) return 0;
    const auto& p = props[i];
    if (auto l = std::get_if<int64_t>(&p)) return *l;
    if (auto d = std::get_if<double>(&p)) return (int64_t)*d;
    if (auto b = std::get_if<bool>(&p)) return *b ? 1 : 0;
    if (auto s = std::get_if<std::string>(&p)) { try { return std::stoll(*s); } catch (...) { return 0; } }
    return 0;
}

double FbxNode::propDouble(size_t i) const {
    if (i >= props.size()) return 0;
    const auto& p = props[i];
    if (auto d = std::get_if<double>(&p)) return *d;
    if (auto l = std::get_if<int64_t>(&p)) return (double)*l;
    if (auto b = std::get_if<bool>(&p)) return *b ? 1 : 0;
    if (auto s = std::get_if<std::string>(&p)) { try { return std::stod(*s); } catch (...) { return 0; } }
    return 0;
}

std::vector<double> FbxNode::asDoubles() const {
    if (props.size() == 1) {
        if (auto d = std::get_if<std::vector<double>>(&props[0])) return *d;
        if (auto l = std::get_if<std::vector<int64_t>>(&props[0])) return std::vector<double>(l->begin(), l->end());
    }
    std::vector<double> r(props.size());
    for (size_t i = 0; i < props.size(); i++) r[i] = propDouble(i);
    return r;
}

std::vector<int64_t> FbxNode::asLongs() const {
    if (props.size() == 1) {
        if (auto l = std::get_if<std::vector<int64_t>>(&props[0])) return *l;
        if (auto d = std::get_if<std::vector<double>>(&props[0])) {
            std::vector<int64_t> r; r.reserve(d->size());
            for (double x : *d) r.push_back((int64_t)x);
            return r;
        }
    }
    std::vector<int64_t> r(props.size());
    for (size_t i = 0; i < props.size(); i++) r[i] = propLong(i);
    return r;
}

const std::vector<uint8_t>* FbxNode::propBytes(size_t i) const {
    if (i >= props.size()) return nullptr;
    return std::get_if<std::vector<uint8_t>>(&props[i]);
}

std::map<std::string, const FbxNode*> FbxNode::properties70() const {
    std::map<std::string, const FbxNode*> r;
    if (auto p70 = child("Properties70"))
        for (auto p : p70->childrenNamed("P")) {
            auto k = p->propString(0);
            if (!k.empty()) r[k] = p;
        }
    return r;
}

std::string fbxCleanName(const std::string& raw) {
    auto sep = raw.find(std::string("\0\x01", 2));
    if (sep != std::string::npos) return raw.substr(0, sep);
    auto c = raw.find("::");
    if (c != std::string::npos) return raw.substr(c + 2);
    return raw;
}

std::vector<uint8_t> readFileBytes(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open " + path);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

// ------------------------------------------------------------------ binary

namespace {

struct BinReader {
    const std::vector<uint8_t>& d;
    size_t pos = 0;
    void need(size_t n) const { if (pos + n > d.size()) throw std::runtime_error("unexpected end of FBX file"); }
    template <class T> T read() { need(sizeof(T)); T v; std::memcpy(&v, d.data() + pos, sizeof(T)); pos += sizeof(T); return v; }
    std::string str(size_t n) { need(n); std::string s((const char*)d.data() + pos, n); pos += n; return s; }
    std::vector<uint8_t> bytes(size_t n) { need(n); std::vector<uint8_t> b(d.begin() + pos, d.begin() + pos + n); pos += n; return b; }
};

template <class T>
std::vector<T> readArray(BinReader& br, size_t elem) {
    uint32_t length = br.read<uint32_t>();
    uint32_t encoding = br.read<uint32_t>();
    uint32_t compLen = br.read<uint32_t>();
    auto raw = br.bytes(compLen);
    std::vector<uint8_t> bytes;
    if (encoding == 1) {
        bytes.resize((size_t)length * elem);
        int got = bytes.empty() ? 0 : stbi_zlib_decode_buffer((char*)bytes.data(), (int)bytes.size(), (const char*)raw.data(), (int)raw.size());
        if (got < 0) throw std::runtime_error("corrupt compressed array in FBX file");
    } else {
        bytes = std::move(raw);
    }
    if (bytes.size() < (size_t)length * elem) throw std::runtime_error("FBX array shorter than declared");
    std::vector<T> out(length);
    if (length) std::memcpy(out.data(), bytes.data(), (size_t)length * elem);
    return out;
}

FbxProp readBinaryProp(BinReader& br) {
    char t = (char)br.read<uint8_t>();
    switch (t) {
        case 'Y': return (int64_t)br.read<int16_t>();
        case 'C': return br.read<uint8_t>() != 0;
        case 'I': return (int64_t)br.read<int32_t>();
        case 'F': return (double)br.read<float>();
        case 'D': return br.read<double>();
        case 'L': return br.read<int64_t>();
        case 'S': { uint32_t n = br.read<uint32_t>(); return br.str(n); }
        case 'R': { uint32_t n = br.read<uint32_t>(); return br.bytes(n); }
        case 'f': { auto a = readArray<float>(br, 4); return std::vector<double>(a.begin(), a.end()); }
        case 'd': return readArray<double>(br, 8);
        case 'l': return readArray<int64_t>(br, 8);
        case 'i': { auto a = readArray<int32_t>(br, 4); return std::vector<int64_t>(a.begin(), a.end()); }
        case 'b': { auto a = readArray<uint8_t>(br, 1); return std::vector<int64_t>(a.begin(), a.end()); }
        default: throw std::runtime_error(std::string("unknown FBX property type '") + t + "'");
    }
}

std::unique_ptr<FbxNode> readBinaryNode(BinReader& br, bool wide, int depth) {
    if (depth > 64) throw std::runtime_error("FBX nesting too deep");
    uint64_t end = wide ? br.read<uint64_t>() : br.read<uint32_t>();
    uint64_t numProps = wide ? br.read<uint64_t>() : br.read<uint32_t>();
    uint64_t propLen = wide ? br.read<uint64_t>() : br.read<uint32_t>();
    uint8_t nameLen = br.read<uint8_t>();
    (void)propLen;
    if (end == 0 && numProps == 0 && nameLen == 0) return nullptr;
    if (end > br.d.size()) throw std::runtime_error("FBX node runs past end of file");
    auto node = std::make_unique<FbxNode>();
    node->name = br.str(nameLen);
    for (uint64_t i = 0; i < numProps; i++) node->props.push_back(readBinaryProp(br));
    while (br.pos < end) {
        auto c = readBinaryNode(br, wide, depth + 1);
        if (!c) break;
        node->children.push_back(std::move(c));
    }
    br.pos = (size_t)end;
    return node;
}

// ------------------------------------------------------------------ ascii

enum class Tok { Name, Value, Comma, Open, Close, NewLine, Star, End };

struct Lexer {
    const std::string& s;
    size_t i = 0;
    Tok kind = Tok::End;
    std::string text;
    FbxProp value;

    explicit Lexer(const std::string& src) : s(src) { next(); }

    void word() {
        size_t st = i;
        while (i < s.size()) {
            char c = s[i];
            if (isspace((unsigned char)c) || c == ',' || c == '{' || c == '}' || c == ':' || c == '"' || c == ';') break;
            i++;
        }
        text = s.substr(st, i - st);
    }

    void next() {
        while (i < s.size()) {
            char c = s[i];
            if (c == ';') { while (i < s.size() && s[i] != '\n') i++; continue; }
            if (c == '\n') { i++; kind = Tok::NewLine; return; }
            if (isspace((unsigned char)c)) { i++; continue; }
            break;
        }
        if (i >= s.size()) { kind = Tok::End; return; }
        char c = s[i];
        if (c == ',') { i++; kind = Tok::Comma; return; }
        if (c == '{') { i++; kind = Tok::Open; return; }
        if (c == '}') { i++; kind = Tok::Close; return; }
        if (c == '*') { i++; word(); kind = Tok::Star; return; }
        if (c == '"') {
            size_t st = ++i;
            while (i < s.size() && s[i] != '"') i++;
            text = s.substr(st, i - st);
            value = text;
            i++;
            kind = Tok::Value;
            return;
        }
        word();
        if (i < s.size() && s[i] == ':') { i++; kind = Tok::Name; return; }
        kind = Tok::Value;
        char* endp = nullptr;
        long long l = strtoll(text.c_str(), &endp, 10);
        if (!text.empty() && *endp == 0) { value = (int64_t)l; return; }
        double d = strtod(text.c_str(), &endp);
        if (!text.empty() && *endp == 0) { value = d; return; }
        value = text;
    }
};

void parseAsciiChildren(Lexer& lx, FbxNode& parent, bool top, int depth);

std::unique_ptr<FbxNode> parseAsciiNode(Lexer& lx, int depth) {
    auto node = std::make_unique<FbxNode>();
    node->name = lx.text;
    lx.next();
    bool isArray = false;
    for (;;) {
        if (lx.kind == Tok::Value) { node->props.push_back(lx.value); lx.next(); }
        else if (lx.kind == Tok::Star) { isArray = true; lx.next(); }
        if (lx.kind == Tok::Comma) {
            lx.next();
            while (lx.kind == Tok::NewLine) lx.next();
            continue;
        }
        break;
    }
    if (lx.kind == Tok::Open) { lx.next(); parseAsciiChildren(lx, *node, false, depth + 1); }
    else if (lx.kind == Tok::NewLine) lx.next();

    if (isArray) {
        const FbxNode* a = node->child("a");
        std::vector<FbxProp> flat;
        if (a) {
            bool ints = true;
            for (auto& p : a->props) if (!std::holds_alternative<int64_t>(p)) { ints = false; break; }
            if (ints) flat.push_back(a->asLongs()); else flat.push_back(a->asDoubles());
        }
        node->children.clear();
        node->props = std::move(flat);
    }
    return node;
}

void parseAsciiChildren(Lexer& lx, FbxNode& parent, bool top, int depth) {
    if (depth > 64) throw std::runtime_error("FBX nesting too deep");
    for (;;) {
        switch (lx.kind) {
            case Tok::End:
                if (!top) throw std::runtime_error("unexpected end of ASCII FBX file");
                return;
            case Tok::Close:
                if (top) throw std::runtime_error("unbalanced '}' in ASCII FBX file");
                lx.next();
                return;
            case Tok::Name:
                parent.children.push_back(parseAsciiNode(lx, depth));
                break;
            default:
                lx.next();
        }
    }
}

} // namespace

FbxDocument fbxParse(const std::vector<uint8_t>& data) {
    FbxDocument doc;
    static const char magic[] = "Kaydara FBX Binary";
    if (data.size() >= 27 && std::memcmp(data.data(), magic, 18) == 0) {
        doc.binary = true;
        BinReader br{data};
        br.pos = 23;
        doc.version = (int)br.read<uint32_t>();
        bool wide = doc.version >= 7500;
        while (br.pos < data.size()) {
            auto n = readBinaryNode(br, wide, 0);
            if (!n) break;
            doc.root.children.push_back(std::move(n));
        }
        return doc;
    }
    std::string text(data.begin(), data.end());
    Lexer lx(text);
    parseAsciiChildren(lx, doc.root, true, 0);
    doc.version = 7400;
    if (auto h = doc.root.child("FBXHeaderExtension"))
        if (auto v = h->child("FBXVersion")) doc.version = (int)v->propLong(0);
    return doc;
}

// ------------------------------------------------------------------ mesh extraction

void MeshData::bounds(Vec3& mn, Vec3& mx) const {
    if (positions.empty()) { mn = mx = Vec3(0); return; }
    mn = Vec3(1e30f); mx = Vec3(-1e30f);
    for (auto& p : positions) { mn = vmin(mn, p); mx = vmax(mx, p); }
}

namespace {

using Props = std::map<std::string, const FbxNode*>;

Vec3 p3(const Props& props, const char* name, Vec3 def) {
    auto it = props.find(name);
    if (it == props.end() || it->second->props.size() < 7) return def;
    auto* p = it->second;
    return {(float)p->propDouble(4), (float)p->propDouble(5), (float)p->propDouble(6)};
}

Mat4 eulerDeg(Vec3 d, int order) {
    Mat4 rx = Mat4::rotX(d.x * kDeg), ry = Mat4::rotY(d.y * kDeg), rz = Mat4::rotZ(d.z * kDeg);
    switch (order) { // 0 XYZ, 1 XZY, 2 YZX, 3 YXZ, 4 ZXY, 5 ZYX; "XYZ" = X applied first
        case 1: return rx * rz * ry;
        case 2: return ry * rz * rx;
        case 3: return ry * rx * rz;
        case 4: return rz * rx * ry;
        case 5: return rz * ry * rx;
        default: return rx * ry * rz;
    }
}

Mat4 localTransform(const FbxNode& model) {
    auto props = model.properties70();
    int order = 0;
    if (auto it = props.find("RotationOrder"); it != props.end()) order = (int)it->second->propLong(4);
    Vec3 t = p3(props, "Lcl Translation", Vec3(0)), r = p3(props, "Lcl Rotation", Vec3(0)), s = p3(props, "Lcl Scaling", Vec3(1));
    Vec3 pre = p3(props, "PreRotation", Vec3(0)), post = p3(props, "PostRotation", Vec3(0));
    Vec3 roff = p3(props, "RotationOffset", Vec3(0)), rpiv = p3(props, "RotationPivot", Vec3(0));
    Vec3 soff = p3(props, "ScalingOffset", Vec3(0)), spiv = p3(props, "ScalingPivot", Vec3(0));
    // FBX: T * Roff * Rp * Rpre * R * Rpost^-1 * Rp^-1 * Soff * Sp * S * Sp^-1 (column vectors), reversed for row vectors
    return Mat4::translation(-spiv) * Mat4::scale(s) * Mat4::translation(spiv) * Mat4::translation(soff) *
           Mat4::translation(-rpiv) * eulerDeg(post, 0).inverted() * eulerDeg(r, order) * eulerDeg(pre, 0) *
           Mat4::translation(rpiv) * Mat4::translation(roff) * Mat4::translation(t);
}

Mat4 geometricTransform(const FbxNode& model) {
    auto props = model.properties70();
    return Mat4::scale(p3(props, "GeometricScaling", Vec3(1))) * eulerDeg(p3(props, "GeometricRotation", Vec3(0)), 0) *
           Mat4::translation(p3(props, "GeometricTranslation", Vec3(0)));
}

Mat4 axisConversion(const FbxDocument& doc) {
    Props gs;
    if (auto g = doc.root.child("GlobalSettings")) gs = g->properties70();
    auto get = [&](const char* n, int def) { auto it = gs.find(n); return it != gs.end() && it->second->props.size() > 4 ? (int)it->second->propLong(4) : def; };
    double unit = 1.0;
    if (auto it = gs.find("UnitScaleFactor"); it != gs.end() && it->second->props.size() > 4) unit = it->second->propDouble(4);
    if (unit <= 0) unit = 1.0;
    int up = get("UpAxis", 1), upS = get("UpAxisSign", 1), fr = get("FrontAxis", 2), frS = get("FrontAxisSign", 1), co = get("CoordAxis", 0), coS = get("CoordAxisSign", 1);
    auto axis = [](int a, int sign) { Vec3 v = a == 0 ? Vec3(1, 0, 0) : a == 1 ? Vec3(0, 1, 0) : Vec3(0, 0, 1); return sign < 0 ? -v : v; };
    Vec3 right = axis(co, coS), upv = axis(up, upS), front = axis(fr, frS);
    if (up == fr || up == co || fr == co) { right = {1, 0, 0}; upv = {0, 1, 0}; front = {0, 0, 1}; }
    Mat4 m;
    m.m[0][0] = right.x; m.m[0][1] = upv.x; m.m[0][2] = front.x;
    m.m[1][0] = right.y; m.m[1][1] = upv.y; m.m[1][2] = front.y;
    m.m[2][0] = right.z; m.m[2][1] = upv.z; m.m[2][2] = front.z;
    return m * Mat4::scale((float)(unit / 100.0));
}

struct Layer {
    std::string mapping = "ByPolygonVertex";
    bool indexed = false;
    std::vector<double> data;
    std::vector<int64_t> index;
    int stride = 0;
    void get(int cp, int pv, int poly, float* out) const {
        long long i = (mapping == "ByVertice" || mapping == "ByVertex" || mapping == "ByControlPoint") ? cp
                    : mapping == "ByPolygon" ? poly : mapping == "AllSame" ? 0 : pv;
        if (indexed) i = (i >= 0 && i < (long long)index.size()) ? index[i] : 0;
        for (int k = 0; k < stride; k++) {
            size_t b = (size_t)i * stride + k;
            out[k] = (i >= 0 && b < data.size()) ? (float)data[b] : 0.f;
        }
    }
};

bool readLayer(const FbxNode& geo, const char* el, const char* dataName, const char* indexName, int stride, Layer& out) {
    auto e = geo.child(el);
    if (!e) return false;
    auto d = e->child(dataName);
    if (!d) return false;
    out.stride = stride;
    out.data = d->asDoubles();
    if (auto m = e->child("MappingInformationType")) out.mapping = m->propString(0);
    std::string ref = "Direct";
    if (auto r = e->child("ReferenceInformationType")) ref = r->propString(0);
    auto idx = e->child(indexName);
    if (!idx && std::string(indexName) == "NormalsIndex") idx = e->child("NormalIndex");
    if ((ref == "IndexToDirect" || ref == "Index") && idx) { out.indexed = true; out.index = idx->asLongs(); }
    return out.data.size() >= (size_t)stride;
}

void appendGeometry(MeshData& mesh, const FbxNode& geo, const Mat4& nodeToFile, const Mat4& axis) {
    auto vn = geo.child("Vertices");
    auto pn = geo.child("PolygonVertexIndex");
    if (!vn || !pn) return;
    auto verts = vn->asDoubles();
    auto poly = pn->asLongs();
    Mat4 full = nodeToFile * axis;
    Mat4 normalM = full.inverted().transposed();

    Layer nl, ul, cl;
    bool hasN = readLayer(geo, "LayerElementNormal", "Normals", "NormalsIndex", 3, nl);
    bool hasU = readLayer(geo, "LayerElementUV", "UV", "UVIndex", 2, ul);
    bool hasC = readLayer(geo, "LayerElementColor", "Colors", "ColorIndex", 4, cl);
    mesh.hasNormals |= hasN; mesh.hasUVs |= hasU; mesh.hasColors |= hasC;

    int cps = (int)(verts.size() / 3);
    std::vector<int> polygon, pvIds;
    int polyIndex = 0;
    for (size_t pv = 0; pv < poly.size(); pv++) {
        long long idx = poly[pv];
        bool end = idx < 0;
        if (end) idx = ~idx;
        polygon.push_back((int)idx);
        pvIds.push_back((int)pv);
        if (!end) continue;
        if (polygon.size() >= 3) {
            int base = (int)mesh.positions.size();
            for (size_t k = 0; k < polygon.size(); k++) {
                int cp = polygon[k];
                if (cp < 0 || cp >= cps) { mesh.warnings.push_back("invalid vertex index"); cp = 0; }
                Vec3 p = full.point({(float)verts[cp * 3], (float)verts[cp * 3 + 1], (float)verts[cp * 3 + 2]});
                p.x = -p.x; // Unity is left-handed
                mesh.positions.push_back(p);
                float tmp[4];
                if (hasN) {
                    nl.get(cp, pvIds[k], polyIndex, tmp);
                    Vec3 n = normalM.vector({tmp[0], tmp[1], tmp[2]});
                    n.x = -n.x;
                    mesh.normals.push_back(n.lengthSq() > 1e-12f ? normalize(n) : Vec3(0, 1, 0));
                } else mesh.normals.push_back({0, 1, 0});
                if (hasU) { ul.get(cp, pvIds[k], polyIndex, tmp); mesh.uvs.push_back({tmp[0], tmp[1]}); }
                else mesh.uvs.push_back({0, 0});
                if (hasC) { cl.get(cp, pvIds[k], polyIndex, tmp); mesh.colors.push_back({tmp[0], tmp[1], tmp[2], tmp[3]}); }
                else mesh.colors.push_back({1, 1, 1, 1});
            }
            for (size_t k = 1; k + 1 < polygon.size(); k++) { // fan, winding flipped for the mirror
                mesh.indices.push_back(base);
                mesh.indices.push_back(base + (int)k + 1);
                mesh.indices.push_back(base + (int)k);
            }
        }
        polygon.clear();
        pvIds.clear();
        polyIndex++;
    }
}

} // namespace

MeshData fbxExtractMesh(const FbxDocument& doc) {
    MeshData mesh;
    auto objects = doc.root.child("Objects");
    if (!objects) throw std::runtime_error("FBX file has no Objects section");
    std::map<int64_t, const FbxNode*> geos, models;
    for (auto& n : objects->children) {
        if (n->name == "Geometry" && n->propString(2) == "Mesh") geos[n->propLong(0)] = n.get();
        else if (n->name == "Model") models[n->propLong(0)] = n.get();
        else if (n->name == "Video" && mesh.embeddedTexture.empty()) {
            if (auto c = n->child("Content")) if (auto b = c->propBytes(0); b && b->size() > 8) mesh.embeddedTexture = *b;
        }
    }
    std::map<int64_t, std::vector<int64_t>> parents;
    if (auto conns = doc.root.child("Connections"))
        for (auto c : conns->childrenNamed("C"))
            if (c->propString(0) == "OO") parents[c->propLong(1)].push_back(c->propLong(2));

    Mat4 axis = axisConversion(doc);
    std::map<int64_t, Mat4> cache;
    std::function<Mat4(int64_t, int)> global = [&](int64_t id, int depth) -> Mat4 {
        if (auto it = cache.find(id); it != cache.end()) return it->second;
        Mat4 local = localTransform(*models[id]);
        Mat4 r = local;
        if (depth < 64)
            if (auto it = parents.find(id); it != parents.end())
                for (auto p : it->second)
                    if (models.count(p)) { r = local * global(p, depth + 1); break; }
        cache[id] = r;
        return r;
    };

    for (auto& [gid, geo] : geos) {
        std::vector<int64_t> owners;
        if (auto it = parents.find(gid); it != parents.end())
            for (auto p : it->second) if (models.count(p)) owners.push_back(p);
        if (owners.empty()) { appendGeometry(mesh, *geo, Mat4::identity(), axis); continue; }
        for (auto m : owners) appendGeometry(mesh, *geo, geometricTransform(*models[m]) * global(m, 0), axis);
    }
    if (mesh.positions.empty()) mesh.warnings.push_back("No mesh geometry found in FBX file.");
    return mesh;
}

MeshData fbxLoadMesh(const std::string& path) { return fbxExtractMesh(fbxParse(readFileBytes(path))); }

} // namespace cw
