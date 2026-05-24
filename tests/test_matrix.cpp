#include "fontscope/matrix.h"
#include <cmath>
#include <cstdio>
#include <cassert>

using namespace fontscope;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "FAIL: %s (line %d)\n", #cond, __LINE__); ++failures; } } while(0)
#define CHECK_NEAR(a,b,eps) CHECK(std::fabs(double(a)-double(b)) < (eps))

static void test_matrix2x2_identity() {
    Matrix2x2 m = Matrix2x2::identity();
    CHECK(m.is_identity());
    CHECK_NEAR(m.scale_x(), 1.0, 1e-5);
    CHECK_NEAR(m.scale_y(), 1.0, 1e-5);
}

static void test_matrix2x2_scale() {
    Matrix2x2 m = Matrix2x2::scale(Fixed16::from_f64(2.0), Fixed16::from_f64(3.0));
    CHECK(!m.is_identity());
    CHECK_NEAR(m.scale_x(), 2.0, 1e-4);
    CHECK_NEAR(m.scale_y(), 3.0, 1e-4);
    CHECK(m.is_uniform_scale() == false);
}

static void test_matrix2x2_uniform_scale() {
    Matrix2x2 m = Matrix2x2::scale(Fixed16::from_f64(2.0), Fixed16::from_f64(2.0));
    CHECK(m.is_uniform_scale());
}

static void test_matrix2x2_multiply() {
    Matrix2x2 a = Matrix2x2::scale(Fixed16::from_f64(2.0), Fixed16::from_f64(1.0));
    Matrix2x2 b = Matrix2x2::scale(Fixed16::from_f64(3.0), Fixed16::from_f64(4.0));
    Matrix2x2 c = a * b;
    CHECK_NEAR(c.xx.to_f64(), 6.0, 1e-3);
    CHECK_NEAR(c.yy.to_f64(), 4.0, 1e-3);
}

static void test_matrix2x2_transform_point() {
    Matrix2x2 m = Matrix2x2::scale(Fixed16::from_f64(2.0), Fixed16::from_f64(3.0));
    FPoint p{10, 20, true};
    FPoint r = m.transform(p);
    CHECK(r.x == 20);
    CHECK(r.y == 60);
}

static void test_matrix2x2_rotation() {
    // 90-degree counter-clockwise rotation: xx=0, xy=-1, yx=1, yy=0
    Matrix2x2 m = Matrix2x2::rotation(3.14159265358979 / 2.0);
    FPoint p{100, 0, true};
    FPoint r = m.transform(p);
    CHECK_NEAR(double(r.x),   0.0, 2.0);
    CHECK_NEAR(double(r.y), 100.0, 2.0);
}

static void test_transform2d_identity() {
    Transform2D t = Transform2D::identity();
    FPoint p{10, 20, true};
    FPoint r = t.apply(p);
    CHECK(r.x == 10);
    CHECK(r.y == 20);
    CHECK(r.on_curve == true);
}

static void test_transform2d_translate() {
    Transform2D t = Transform2D::translate(100, 200);
    FPoint p{10, 20, true};
    FPoint r = t.apply(p);
    CHECK(r.x == 110);
    CHECK(r.y == 220);
}

static void test_transform2d_scale_translate() {
    Transform2D t = Transform2D::scale_translate(
        Fixed16::from_f64(2.0), Fixed16::from_f64(2.0), 5, 5);
    FPoint p{10, 10, true};
    FPoint r = t.apply(p);
    CHECK(r.x == 25);
    CHECK(r.y == 25);
}

static void test_transform2d_compose() {
    Transform2D a = Transform2D::translate(10, 0);
    Transform2D b = Transform2D::translate(0, 20);
    Transform2D c = a * b;
    FPoint p{0, 0, true};
    FPoint r = c.apply(p);
    CHECK(r.x == 10);
    CHECK(r.y == 20);
}

static void test_transform_bbox() {
    Transform2D t = Transform2D::translate(100, 200);
    FBBox bbox{0, 0, 100, 50};
    FBBox r = t.transform_bbox(bbox);
    CHECK(r.x_min == 100);
    CHECK(r.y_min == 200);
    CHECK(r.x_max == 200);
    CHECK(r.y_max == 250);
}

static void test_decompose_identity() {
    Transform2D t = Transform2D::identity();
    MatrixDecomposition d = decompose_matrix(t.m);
    CHECK_NEAR(d.scale_x, 1.0, 1e-4);
    CHECK_NEAR(d.scale_y, 1.0, 1e-4);
    CHECK_NEAR(d.rotation, 0.0, 0.1);
    CHECK_NEAR(d.skew, 0.0, 1e-4);
}

static void test_matrix_from_rotation_deg() {
    Matrix2x2 m = matrix_from_rotation_deg(90.0);
    FPoint p{1, 0, true};
    FPoint r = m.transform(p);
    CHECK_NEAR(double(r.x),  0.0, 1.0);
    CHECK_NEAR(double(r.y),  1.0, 1.0);
}

static void test_matrix_det() {
    Matrix2x2 m = Matrix2x2::scale(Fixed16::from_f64(3.0), Fixed16::from_f64(4.0));
    double det = m.det().to_f64();
    CHECK_NEAR(det, 12.0, 0.1);
}

int main() {
    test_matrix2x2_identity();
    test_matrix2x2_scale();
    test_matrix2x2_uniform_scale();
    test_matrix2x2_multiply();
    test_matrix2x2_transform_point();
    test_matrix2x2_rotation();
    test_transform2d_identity();
    test_transform2d_translate();
    test_transform2d_scale_translate();
    test_transform2d_compose();
    test_transform_bbox();
    test_decompose_identity();
    test_matrix_from_rotation_deg();
    test_matrix_det();

    if (failures) {
        fprintf(stderr, "test_matrix: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_matrix: all passed\n");
    return 0;
}
