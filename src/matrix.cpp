#include "fontscope/matrix.h"
#include <cmath>

namespace fontscope {

MatrixDecomposition decompose_matrix(const Matrix2x2& m) {
    MatrixDecomposition d{};

    double a = m.xx.to_f64();
    double b = m.xy.to_f64();
    double c = m.yx.to_f64();
    double e = m.yy.to_f64();

    double det = a*e - b*c;
    d.is_reflection = det < 0.0;

    d.scale_x = std::sqrt(a*a + b*b);
    if (d.is_reflection && d.scale_x != 0.0) {
        // Flip to correct for reflection.
        a = -a; b = -b;
        d.scale_x = -d.scale_x;
    }

    double r_angle = std::atan2(b, a);
    d.rotation = r_angle * (180.0 / M_PI);

    double cos_r = std::cos(r_angle);
    double sin_r = std::sin(r_angle);

    double a1 = cos_r * e - sin_r * c;
    double b1 = cos_r * b - sin_r * a;

    d.scale_y = a1;
    d.skew    = (d.scale_x != 0.0) ? std::atan(b1 / d.scale_x) * (180.0 / M_PI)
                                     : 0.0;

    return d;
}

Matrix2x2 matrix_from_rotation_deg(double degrees) {
    double rad = degrees * M_PI / 180.0;
    return Matrix2x2::rotation(rad);
}

double matrix_scale_at(const Matrix2x2& m, double dir_x, double dir_y) {
    double len = std::sqrt(dir_x*dir_x + dir_y*dir_y);
    if (len == 0.0) return 0.0;
    dir_x /= len; dir_y /= len;

    double rx = m.xx.to_f64() * dir_x + m.xy.to_f64() * dir_y;
    double ry = m.yx.to_f64() * dir_x + m.yy.to_f64() * dir_y;
    return std::sqrt(rx*rx + ry*ry);
}

} // namespace fontscope
