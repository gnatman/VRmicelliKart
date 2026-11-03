#include <libultraship/bridge.h>

#include <array>
#include <memory>
#include <vector>
#include <map>
#include <unordered_map>
#include <math.h>
#include "port/Engine.h"
#include "FrameInterpolation.h"
#include "matrix.h"
#include "engine/Matrix.h"
#include "src/engine/CoreMath.h"

extern "C" {
#include "math_util.h"
#include "math_util_2.h"
#include "render_player.h"

extern Mat4* gInterpolationMatrix;
void mtxf_translate(Mat4, Vec3f);
}
/*
Frame interpolation.

The idea of this code is to interpolate all matrices.

The code contains two approaches. The first is to interpolate
all inputs in transformations, such as angles, scale and distances,
and then perform the same transformations with the interpolated values.
After evaluation for some reason some animations such rolling look strange.

The second approach is to simply interpolate the final matrices. This will
more or less simply interpolate the world coordinates for movements.
This will however make rotations ~180 degrees get the "paper effect".
The mitigation is to identify this case for actors and interpolate the
matrix but in model coordinates instead, by "removing" the rotation-
translation before interpolating, create a rotation matrix with the
interpolated angle which is then applied to the matrix.

Currently the code contains both methods but only the second one is currently
used.

Both approaches build a tree of instructions, containing matrices
at leaves. Every node is built from OPEN_DISPS/CLOSE_DISPS and manually
inserted FrameInterpolation_OpenChild/FrameInterpolation_Close child calls.
These nodes contain information that should suffice to identify the matrix,
so we can find it in an adjacent frame.

We can interpolate an arbitrary amount of frames between two original frames,
given a specific interpolation factor (0=old frame, 0.5=average of frames,
1.0=new frame).
*/

static bool invert_matrix(const float m[16], float invOut[16]);

using namespace std;


namespace {

enum class Op {
    Marker,
    OpenChild,
    CloseChild,

    MatrixPush,
    MatrixPop,
    MatrixPut,
    MatrixMult,
    MatrixTranslate,
    MatrixScale,
    MatrixRotate1Coord,
    MatrixMult4x4,
    MatrixPosRotXYZ,
    MatrixMultVec3fNoTranslate,
    MatrixMultVec3f,
    MatrixMtxFToMtx,
    MatrixToMtx,
    MatrixRotateAxis,
    SkinMatrixMtxFToMtx,
    SetTransformMatrix,
    SetMatrixTransformation,
    SetTranslateRotate,
    SetTextMatrix,
    SetMatrixPosRotScaleXY,
    SetApplyMatrixTransformations,
    Count,
};

static constexpr size_t kOpCount = static_cast<size_t>(Op::Count);

typedef pair<const void*, uintptr_t> label;

union Data {
    Data() {
    }

    struct {
        Mat4** matrix;
    } matrix_ptr;

    struct {
        const char* file;
        int line;
    } marker;

    struct {
        Mat4* matrix;
        MtxF mf;
        u8 mode;
    } matrix_mult;

    struct {
        Mat4* matrix;
        FVector pos;
        IRotator rot;
        FVector scale;
    } matrix_applytransformations;

    struct {
        Mat4* matrix;
        Vec3fInterp b;
    } matrix_translate;

    struct {
        Mat4* matrix;
        f32 scale;
    } matrix_scale;

    struct {
        Mat4* matrix;
        u32 coord;
        s16 value;
    } matrix_rotate_1_coord;

    struct {
        Mat4* matrix;
        Vec3f src;
        Vec3f dest;
    } matrix_vec_translate;

    struct {
        Mat4* matrix;
        Vec3f src;
        Vec3f dest;
    } matrix_vec_no_translate;

    struct {
        Mat4* dest;
        Mat4 mtx1;
        Mat4 mtx2;
    } matrix_mult_4x4;

    struct {
        Mat4* matrix;
        Vec3fInterp pos;
        Vec3sInterp orientation;
    } matrix_pos_rot_xyz;

    struct {
        Mat4* matrix;
        s32 x;
        s32 y;
        u16 angle;
        f32 scale;
    } matrix_pos_rot_scale_xy;

    struct {
        Mat4* matrix;
        Vec3f translation;
        Vec3s rotation;
    } matrix_translate_rotate_zyx;

    struct {
        Mat4* matrix;
        f32 translateX, translateY, translateZ;
        Vec3s rot;
        // MtxF mtx;
        bool has_mtx;
    } matrix_set_translate_rotate_yxz;

    struct {
        MtxF src;
        Mtx* dest;
    } matrix_mtxf_to_mtx;

    struct {
        Mtx* dest;
        MtxF src;
        bool has_adjusted;
    } matrix_to_mtx;

    struct {
        MtxF mf;
    } matrix_replace_rotation;

    struct {
        f32 angle;
        Vec3f axis;
        u8 mode;
    } matrix_rotate_axis;

    struct {
        Mat4* dest;
        Vec3f orientationVector;
        Vec3f positionVector;
        u16 rotationAngle;
        f32 scaleFactor;
    } set_transform_matrix_data;

    struct {
        Mat4* dest;
        Vec3f location;
        Vec3su rotation;
        f32 scale;
    } set_matrix_transformation_data;

    struct {
        Mat4* dest;
        Vec3f location;
        Vec3s rotation;
    } set_translate_rotate_data;

    struct {
        Mat3* dest;
        f32 arg1;
        f32 arg2;
        f32 arg3;
        s16 rot;
    } set_orientation_matrix_data;

    struct {
        Mat4* matrix;
        f32 x;
        f32 y;
        f32 arg3;
        f32 arg4;
    } matrix_text;

    struct {
        label key;
        size_t idx;
    } open_child;
};

struct LabelHash {
    size_t operator()(const label& k) const noexcept {
        size_t h1 = reinterpret_cast<uintptr_t>(k.first);
        size_t h2 = k.second;
        return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
    }
};

struct Path;

struct ChildSlot {
    vector<unique_ptr<Path>> paths;
    size_t count = 0;

    ChildSlot() = default;
    ChildSlot(ChildSlot&&) noexcept = default;
    ChildSlot& operator=(ChildSlot&&) noexcept = default;
    ~ChildSlot();
};

struct Path {
    unordered_map<label, ChildSlot, LabelHash> children;
    array<vector<Data>, kOpCount> ops;
    vector<pair<Op, size_t>> items;

    void Reset() {
        items.clear();
        for (auto& opVec : ops) {
            opVec.clear();
        }
        if (children.size() > 2048) {
            children.clear();
        } else {
            for (auto& kv : children) {
                for (size_t i = 0; i < kv.second.count; ++i) {
                    kv.second.paths[i]->Reset();
                }
                kv.second.count = 0;
            }
        }
    }
};

ChildSlot::~ChildSlot() = default;

struct Recording {
    Path root_path;
};

bool is_recording;
vector<Path*> current_path;
uint32_t camera_epoch;
uint32_t previous_camera_epoch;
Recording current_recording;
Recording previous_recording;

bool next_is_actor_pos_rot_matrix;
bool has_inv_actor_mtx;
MtxF inv_actor_mtx;
size_t inv_actor_mtx_path_index;

Data& append(Op op) {
    auto& m = current_path.back()->ops[static_cast<size_t>(op)];
    current_path.back()->items.emplace_back(op, m.size());
    return m.emplace_back();
}

MtxF* Matrix_GetCurrent() {
    return (MtxF*) gInterpolationMatrix;
}

struct InterpolateCtx {
    float step = 0.0f;
    float w = 1.0f;
    unordered_map<Mtx*, MtxF>& mtx_replacements;
    MtxF tmp_mtxf;

    explicit InterpolateCtx(unordered_map<Mtx*, MtxF>& out) : mtx_replacements(out) {}

    MtxF* new_replacement(Mtx* addr) {
        return &mtx_replacements[addr];
    }

    void interpolate_mtxf(MtxF* res, MtxF* o, MtxF* n) {
        for (size_t i = 0; i < 4; i++) {
            for (size_t j = 0; j < 4; j++) {
                res->mf[i][j] = w * o->mf[i][j] + step * n->mf[i][j];
            }
        }
    }

    float lerp(f32 o, f32 n) {
        return w * o + step * n;
    }

    s16 lerp_s16(s16 o, s16 n) {
        return w * o + step * n;
    }

    s32 lerp_s32(s32 o, s32 n) {
        return w * o + step * n;
    }

    void lerp_vec3s(Vec3s* res, Vec3s o, Vec3s n) {
        (*res)[0] = lerp_s16(o[0], n[0]);
        (*res)[1] = lerp_s16(o[1], n[1]);
        (*res)[2] = lerp_s16(o[2], n[2]);
    }

    void lerp_vec3f(Vec3f* res, Vec3f* o, Vec3f* n) {
        (*res)[0] = lerp((*o)[0], (*n)[0]);
        (*res)[1] = lerp((*o)[1], (*n)[1]);
        (*res)[2] = lerp((*o)[2], (*n)[2]);
    }

    float interpolate_angle(f32 o, f32 n) {
        if (o == n)
            return n;
        o = fmodf(o, 2 * M_PI);
        if (o < 0.0f) {
            o += 2 * M_PI;
        }
        n = fmodf(n, 2 * M_PI);
        if (n < 0.0f) {
            n += 2 * M_PI;
        }
        if (fabsf(o - n) > M_PI) {
            if (o < n) {
                o += 2 * M_PI;
            } else {
                n += 2 * M_PI;
            }
        }
        if (fabsf(o - n) > M_PI / 2) {
            // return n;
        }
        return lerp(o, n);
    }

    s16 interpolate_angle(s16 os, s16 ns) {
        if (os == ns)
            return ns;
        int o = (u16) os;
        int n = (u16) ns;
        u16 res;
        int diff = o - n;
        if (-0x8000 <= diff && diff <= 0x8000) {
            if (diff < -0x4000 || diff > 0x4000) {
                return ns;
            }
            res = (u16) (w * o + step * n);
        } else {
            if (o < n) {
                o += 0x10000;
            } else {
                n += 0x10000;
            }
            diff = o - n;
            if (diff < -0x4000 || diff > 0x4000) {
                return ns;
            }
            res = (u16) (w * o + step * n);
        }
        return res;
    }

    void interpolate_vecs(Vec3f* res, Vec3f* o, Vec3f* n) {
        (*res)[0] = interpolate_angle((*o)[0], (*n)[0]);
        (*res)[1] = interpolate_angle((*o)[1], (*n)[1]);
        (*res)[2] = interpolate_angle((*o)[2], (*n)[2]);
    }

    void interpolate_angles(Vec3s* res, Vec3s* o, Vec3s* n) {
        (*res)[0] = interpolate_angle((*o)[0], (*n)[0]);
        (*res)[1] = interpolate_angle((*o)[1], (*n)[1]);
        (*res)[2] = interpolate_angle((*o)[2], (*n)[2]);
    }

    void interpolate_vec3s(Vec3s* res, Vec3s o, Vec3s n) {
        (*res)[0] = interpolate_angle(o[0], n[0]);
        (*res)[1] = interpolate_angle(o[1], n[1]);
        (*res)[2] = interpolate_angle(o[2], n[2]);
    }

    void interpolate_branch(Path* old_path, Path* new_path) {
        for (auto& item : new_path->items) {
            const size_t opIdx = static_cast<size_t>(item.first);
            Data& new_op = new_path->ops[opIdx][item.second];

            if (item.first == Op::OpenChild) {
                auto newIt = new_path->children.find(new_op.open_child.key);
                Path* newChild = newIt->second.paths[new_op.open_child.idx].get();
                auto oldIt = old_path->children.find(new_op.open_child.key);
                if (oldIt != old_path->children.end() && new_op.open_child.idx < oldIt->second.count) {
                    interpolate_branch(oldIt->second.paths[new_op.open_child.idx].get(), newChild);
                } else {
                    interpolate_branch(newChild, newChild);
                }
                continue;
            }

            auto& oldOpVec = old_path->ops[opIdx];
            if (item.second < oldOpVec.size()) {
                Data& old_op = oldOpVec[item.second];
                switch (item.first) {
                    case Op::MatrixMtxFToMtx:
                        interpolate_mtxf(new_replacement(new_op.matrix_mtxf_to_mtx.dest),
                                         &old_op.matrix_mtxf_to_mtx.src, &new_op.matrix_mtxf_to_mtx.src);
                        break;

                    case Op::MatrixToMtx: {
                        if (old_op.matrix_to_mtx.has_adjusted && new_op.matrix_to_mtx.has_adjusted) {
                            interpolate_mtxf(&tmp_mtxf, &old_op.matrix_to_mtx.src, &new_op.matrix_to_mtx.src);
                        } else {
                            interpolate_mtxf(new_replacement(new_op.matrix_to_mtx.dest), &old_op.matrix_to_mtx.src,
                                             &new_op.matrix_to_mtx.src);
                        }
                        break;
                    }

                    default:
                        break;
                }
            }
        }
    }
};

} // anonymous namespace

void FrameInterpolation_InterpolateInto(float step, unordered_map<Mtx*, MtxF>& out) {
    out.clear();
    InterpolateCtx ctx(out);
    ctx.step = step;
    ctx.w = 1.0f - step;
    ctx.interpolate_branch(&previous_recording.root_path, &current_recording.root_path);
}

unordered_map<Mtx*, MtxF> FrameInterpolation_Interpolate(float step) {
    unordered_map<Mtx*, MtxF> out;
    out.reserve(512);
    FrameInterpolation_InterpolateInto(step, out);
    return out;
}

bool camera_interpolation = true;

static bool IsInterpolationActiveThisTick() {
    if (CVarGetInteger("gModifyInterpolationTargetFPS", 0)) {
        return CVarGetInteger("gInterpolationTargetFPS", 60) != 30;
    }
    return GameEngine::GetInterpolationFPS() != 30;
}

void FrameInterpolation_ShouldInterpolateFrame(bool shouldInterpolate) {
    camera_interpolation = shouldInterpolate;
    is_recording = shouldInterpolate && IsInterpolationActiveThisTick();
}

bool check_if_recording() {
    return is_recording;
}

void FrameInterpolation_StartRecord(void) {
    std::swap(previous_recording, current_recording);
    current_recording.root_path.Reset();
    current_path.clear();
    if (current_path.capacity() < 32) {
        current_path.reserve(32);
    }
    current_path.push_back(&current_recording.root_path);
    if (!camera_interpolation) {
        // default to interpolating
        camera_interpolation = true;
        is_recording = false;
        return;
    }
    is_recording = IsInterpolationActiveThisTick();
}

void FrameInterpolation_StopRecord(void) {
    previous_camera_epoch = camera_epoch;
    is_recording = false;
}

void FrameInterpolation_RecordOpenChild(const void* a, uintptr_t b) {
    if (!is_recording) {
        return;
    }
    label key = { a, b };
    auto& slot = current_path.back()->children[key];
    size_t idx = slot.count++;
    if (idx >= slot.paths.size()) {
        slot.paths.push_back(std::make_unique<Path>());
    }
    append(Op::OpenChild).open_child = { key, idx };
    current_path.push_back(slot.paths[idx].get());
}

void FrameInterpolation_RecordCloseChild(void) {
    if (!is_recording) {
        return;
    }
    if (has_inv_actor_mtx && current_path.size() == inv_actor_mtx_path_index) {
        has_inv_actor_mtx = false;
    }
    current_path.pop_back();
}

void FrameInterpolation_DontInterpolateCamera(void) {
    camera_epoch = previous_camera_epoch + 1;
}

int FrameInterpolation_GetCameraEpoch(void) {
    return (int) camera_epoch;
}

// Method 2 (final MtxF interpolation via FrameInterpolation_RecordMatrixMtxFToMtx) is exclusively used;
// intermediate matrix builder steps below only wrote to the unused sInterpolationMatrixStack scratch buffer.
void FrameInterpolation_Record_SetTextMatrix(Mat4* matrix, f32 x, f32 y, f32 arg3, f32 arg4) {
    (void)matrix; (void)x; (void)y; (void)arg3; (void)arg4;
}

void FrameInterpolation_RecordActorPosRotMatrix(void) {
}

void FrameInterpolation_RecordMarker(const char* file, int line) {
    (void)file; (void)line;
}

void FrameInterpolation_RecordMatrixPut(MtxF* src) {
    (void)src;
}

void FrameInterpolation_RecordMatrixMult(Mat4* matrix, MtxF* mf, u8 mode) {
    (void)matrix; (void)mf; (void)mode;
}

void FrameInterpolation_ApplyMatrixTransformations(Mat4* matrix, FVector pos, IRotator rot, FVector scale) {
    (void)matrix; (void)pos; (void)rot; (void)scale;
}

void FrameInterpolation_RecordMatrixTranslate(Mat4* matrix, Vec3f b) {
    (void)matrix; (void)b;
}

void FrameInterpolation_RecordMatrixScale(Mat4* matrix, f32 scale) {
    (void)matrix; (void)scale;
}

void FrameInterpolation_RecordMatrixMultVec3fNoTranslate(Mat4* matrix, Vec3f src, Vec3f dest) {
    (void)matrix; (void)src; (void)dest;
}

void FrameInterpolation_RecordSetTransformMatrix(Mat4* dest, Vec3f orientationVector, Vec3f positionVector, u16 rotationAngle,
                          f32 scaleFactor) {
    (void)dest; (void)orientationVector; (void)positionVector; (void)rotationAngle; (void)scaleFactor;
}

void FrameInterpolation_RecordTranslateRotate(Mat4* dest, Vec3f pos, Vec3s rotation) {
    (void)dest; (void)pos; (void)rotation;
}

void FrameInterpolation_RecordSetMatrixTransformation(Mat4* dest, Vec3f location, Vec3su rotation, f32 scale) {
    (void)dest; (void)location; (void)rotation; (void)scale;
}

void FrameInterpolation_RecordCalculateOrientationMatrix(Mat3* dest, f32 x, f32 y, f32 z, s16 rot) {
    (void)dest; (void)x; (void)y; (void)z; (void)rot;
}

void FrameInterpolation_RecordMatrixPosRotXYZ(Mat4* out, Vec3f pos, Vec3s orientation) {
    (void)out; (void)pos; (void)orientation;
}

void FrameInterpolation_RecordMatrixPosRotScaleXY(Mat4* matrix, s32 x, s32 y, u16 angle, f32 scale) {
    (void)matrix; (void)x; (void)y; (void)angle; (void)scale;
}

void FrameInterpolation_RecordMatrixMultVec3f(Mat4* matrix, Vec3f src, Vec3f dest) {
    (void)matrix; (void)src; (void)dest;
}

void FrameInterpolation_RecordMatrixRotate1Coord(Mat4* matrix, u32 coord, s16 value) {
    (void)matrix; (void)coord; (void)value;
}

void FrameInterpolation_RecordMatrixMtxFToMtx(MtxF* src, Mtx* dest) {
    if (!is_recording) {
        return;
    }
    append(Op::MatrixMtxFToMtx).matrix_mtxf_to_mtx = { .src = *src, .dest = dest };
}

void FrameInterpolation_RecordMatrixToMtx(Mtx* dest, char* file, s32 line) {
    (void)file; (void)line;
    if (!is_recording) {
        return;
    }
    auto& d = append(Op::MatrixToMtx).matrix_to_mtx = { dest };
    if (has_inv_actor_mtx) {
        d.has_adjusted = true;
    } else {
        d.src = *Matrix_GetCurrent();
    }
}

void FrameInterpolation_RecordMatrixRotateAxis(f32 angle, Vec3f* axis, u8 mode) {
    (void)angle; (void)axis; (void)mode;
}

void FrameInterpolation_RecordSkinMatrixMtxFToMtx(MtxF* src, Mtx* dest) {
    if (!is_recording) {
        return;
    }
    append(Op::MatrixMtxFToMtx).matrix_mtxf_to_mtx = { .src = *src, .dest = dest };
}

// https://stackoverflow.com/questions/1148309/inverting-a-4x4-matrix
static bool invert_matrix(const float m[16], float invOut[16]) {
    float inv[16], det;
    int i;

    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] +
             m[13] * m[6] * m[11] - m[13] * m[7] * m[10];

    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] -
             m[12] * m[6] * m[11] + m[12] * m[7] * m[10];

    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] +
             m[12] * m[5] * m[11] - m[12] * m[7] * m[9];

    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] -
              m[12] * m[5] * m[10] + m[12] * m[6] * m[9];

    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] -
             m[13] * m[2] * m[11] + m[13] * m[3] * m[10];

    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] +
             m[12] * m[2] * m[11] - m[12] * m[3] * m[10];

    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] -
             m[12] * m[1] * m[11] + m[12] * m[3] * m[9];

    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] +
              m[12] * m[1] * m[10] - m[12] * m[2] * m[9];

    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] +
             m[13] * m[2] * m[7] - m[13] * m[3] * m[6];

    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] -
             m[12] * m[2] * m[7] + m[12] * m[3] * m[6];

    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] +
              m[12] * m[1] * m[7] - m[12] * m[3] * m[5];

    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] -
              m[12] * m[1] * m[6] + m[12] * m[2] * m[5];

    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] -
             m[9] * m[2] * m[7] + m[9] * m[3] * m[6];

    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] +
             m[8] * m[2] * m[7] - m[8] * m[3] * m[6];

    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] -
              m[8] * m[1] * m[7] + m[8] * m[3] * m[5];

    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] -
              m[8] * m[2] * m[5];

    det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

    if (det == 0) {
        return false;
    }

    det = 1.0 / det;

    for (i = 0; i < 16; i++) {
        invOut[i] = inv[i] * det;
    }

    return true;
}