#pragma once
#include <cstdint>
#include <cmath>
#include <vector>
#include "fontscope/fixed.h"
#include "fontscope/font_types.h"

namespace fontscope {

// 2x2 matrix for TrueType composite glyph transformations.
struct Matrix2x2 {
    Fixed16 xx, xy;
    Fixed16 yx, yy;

    static Matrix2x2 identity() {
        return {Fixed16::from_int(1), Fixed16::from_int(0),
                Fixed16::from_int(0), Fixed16::from_int(1)};
    }

    static Matrix2x2 scale(Fixed16 sx, Fixed16 sy) {
        return {sx, Fixed16::from_int(0),
                Fixed16::from_int(0), sy};
    }

    static Matrix2x2 rotation(double angle_rad) {
        double c = std::cos(angle_rad);
        double s = std::sin(angle_rad);
        return {Fixed16::from_f64(c),  Fixed16::from_f64(-s),
                Fixed16::from_f64(s),  Fixed16::from_f64(c)};
    }

    static Matrix2x2 shear_x(Fixed16 t) {
        return {Fixed16::from_int(1), t,
                Fixed16::from_int(0), Fixed16::from_int(1)};
    }

    Matrix2x2 operator*(const Matrix2x2& o) const {
        return {xx * o.xx + xy * o.yx, xx * o.xy + xy * o.yy,
                yx * o.xx + yy * o.yx, yx * o.xy + yy * o.yy};
    }

    FPoint transform(const FPoint& p) const {
        Fixed16 x = Fixed16::from_int(p.x);
        Fixed16 y = Fixed16::from_int(p.y);
        return {FUnit((xx*x + xy*y).integer()),
                FUnit((yx*x + yy*y).integer()),
                p.on_curve};
    }

    // Determinant (det = xx*yy - xy*yx)
    Fixed16 det() const { return xx * yy - xy * yx; }

    // Scale factor magnitude (uniform scale approximation)
    double scale_x() const { return std::sqrt(xx.to_f64()*xx.to_f64() + xy.to_f64()*xy.to_f64()); }
    double scale_y() const { return std::sqrt(yx.to_f64()*yx.to_f64() + yy.to_f64()*yy.to_f64()); }
    double rotation_angle() const { return std::atan2(yx.to_f64(), xx.to_f64()); }

    bool is_identity() const {
        return xx == Fixed16::from_int(1) && xy.raw == 0 &&
               yx.raw == 0 && yy == Fixed16::from_int(1);
    }

    bool is_uniform_scale() const {
        return xy.raw == 0 && yx.raw == 0 && xx == yy;
    }
};

// 2D affine transform: matrix + translation.
struct Transform2D {
    Matrix2x2 m;
    FUnit     tx{0}, ty{0};

    static Transform2D identity() { return {Matrix2x2::identity(), 0, 0}; }

    static Transform2D translate(FUnit x, FUnit y) {
        return {Matrix2x2::identity(), x, y};
    }

    static Transform2D scale_translate(Fixed16 sx, Fixed16 sy, FUnit x, FUnit y) {
        return {Matrix2x2::scale(sx, sy), x, y};
    }

    Transform2D operator*(const Transform2D& o) const {
        FPoint o_trans = m.transform({o.tx, o.ty, true});
        return {m * o.m,
                FUnit(o_trans.x + tx),
                FUnit(o_trans.y + ty)};
    }

    FPoint apply(const FPoint& p) const {
        FPoint r = m.transform(p);
        return {FUnit(r.x + tx), FUnit(r.y + ty), p.on_curve};
    }

    void apply_to_outline(std::vector<FPoint>& pts) const {
        for (auto& p : pts) p = apply(p);
    }

    FBBox transform_bbox(FBBox bb) const {
        FPoint corners[4] = {
            {bb.x_min, bb.y_min, true},
            {bb.x_max, bb.y_min, true},
            {bb.x_min, bb.y_max, true},
            {bb.x_max, bb.y_max, true},
        };
        FBBox out{32767, 32767, -32767, -32767};
        for (const auto& c : corners) {
            FPoint t = apply(c);
            out.x_min = std::min(out.x_min, t.x);
            out.y_min = std::min(out.y_min, t.y);
            out.x_max = std::max(out.x_max, t.x);
            out.y_max = std::max(out.y_max, t.y);
        }
        return out;
    }
};

// Decompose a Matrix2x2 into scale + rotation.
struct MatrixDecomposition {
    double scale_x;
    double scale_y;
    double rotation;
    double skew;
    bool is_reflection;
};

MatrixDecomposition decompose_matrix(const Matrix2x2& m);

// Build a rotation matrix from an angle in degrees.
Matrix2x2 matrix_from_rotation_deg(double degrees);

// Approximate the matrix scale at a given direction vector.
double matrix_scale_at(const Matrix2x2& m, double dir_x, double dir_y);

} // namespace fontscope
