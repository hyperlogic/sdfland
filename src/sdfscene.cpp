//
//  SDFScene.cpp
//
//  Created by Anthony J. Thibault on 9/2/15.
//  Copyright (c) 2019 High Fidelity, Inc. All rights reserved.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "sdfscene.h"

#include <algorithm>  // for min & max

#include <stdio.h>
#include <stdlib.h>

#define _USE_MATH_DEFINES // for C++
#include <math.h>

struct Prim {
    int type; // 0 = sphere, 1 = box
    float m[6];
    float inv_m[6];
    float r[2];
};

struct MapResult {
    float dist;
    int nearest_prim;
};

static float sign(float v) {
    return v > 0.0f ? 1.0f : 0.0f;
}

// transform point p by the 2x3 homogenous matrix m
// | m[0] m[2] m[4] |   | p[0] |   | r[0] |
// | m[1] m[3] m[5] | * | p[1] | = | r[1] |
// |   0    0    1  |   |   1  |   |      |
static void xform_2x3(float *r, const float *m, float *p) {
    float temp[2];
    temp[0] = m[0] * p[0] + m[2] * p[1] + m[4];
    temp[1] = m[1] * p[0] + m[3] * p[1] + m[5];
    r[0] = temp[0];
    r[1] = temp[1];
}

// transform p by a 2x2 matrix.
// | m[0] m[2] | * | p[0] | = | r[0] |
// | m[1] m[3] |   | p[1] |   | r[1] |
static void xform_2x2(float *r, float *m, float *p) {
    float temp[2];
    temp[0] = m[0] * p[0] + m[2] * p[1];
    temp[1] = m[1] * p[0] + m[3] * p[1];
    r[0] = temp[0];
    r[1] = temp[1];
}

// transpose a 2x2 matrix
static void transpose_2x2(float *r, float *m) {
    r[0] = m[0];
    float temp = m[1];
    r[1] = m[2];
    r[2] = temp;
    r[3] = m[3];
}

// invert a orthonormal 2x3 matrix.
static void orthonormal_invert_2x3(float *r, float *m) {
    transpose_2x2(r, m);
    float trans[2] = {-m[4], -m[5]};
    xform_2x2(trans, r, trans);
    r[4] = trans[0];
    r[5] = trans[1];
}

static void make_rotation_matrix_2x2(float *r, float theta) {
    r[0] = cosf(theta);
    r[1] = sinf(theta);
    r[2] = r[1];
    r[3] = -r[0];
}

static float sdf_box(float *p, const Prim& prim) {
    // vec2 d = abs(p) - r;
    // return length(max(d, vec2(0))) + min(max(d.x, d.y), 0.0);
    float d[2] = {fabs(p[0]) - prim.r[0], fabs(p[1]) - prim.r[1]};
    float m[2] = {std::max(d[0], 0.0f), std::max(d[1], 0.0f)};
    float a = sqrt(m[0] * m[0] + m[1] * m[1]);
    float b = std::min(std::max(d[0], d[1]), 0.0f);
    return a + b;
}

static float sdf_sphere(float *p, const Prim& prim) {
    // return length(p) - r;
    return sqrtf(p[0] * p[0] + p[1] * p[1]) - prim.r[0];
}

static float sdf_prim(float* p, const Prim& prim) {
    float local_p[2];
    switch (prim.type) {
    default:
    case 0:
        xform_2x3(local_p, prim.inv_m, p);
        return sdf_sphere(local_p, prim);
    case 1:
        // transform from global into local space
        xform_2x3(local_p, prim.inv_m, p);
        return sdf_box(local_p, prim);
    }
}

// evaluate sdf at point p
static MapResult map(const std::vector<Prim>& prims, float* p) {
    int nearest_prim = prims.size();
    float dist = FLT_MAX;
    int i;
    for (i = 0; i < (int)prims.size(); i++) {
        float new_dist = sdf_prim(p, prims[i]);
        if (new_dist < dist) {
            nearest_prim = i;
            dist = new_dist;
        }
    }
    MapResult result;
    result.dist = dist;
    result.nearest_prim = nearest_prim;
    return result;
}

static void draw_sdf_prims(const std::vector<Prim>& prims, int width, int height, float* buffer) {
    int x, y;
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            float *pixel = buffer + (y * width + x);

            // convert from "viewport" coordinates into "world" space
            float p[2] = {(float)x / (float)width, 1.0f - (float)y / (float)height};
            MapResult r = map(prims, p);
            float dist = r.dist;

            *pixel = dist;
        }
    }
}

SDFScene::SDFScene() {
    _width = 128;
    _height = 128;
    _buffer = new float[_width * _height];

    // ground
    Prim prim;
    prim.type = 1;
    make_rotation_matrix_2x2(prim.m, 0.0f);
    prim.m[4] = 0.0f;
    prim.m[5] = -0.9f;
    orthonormal_invert_2x3(prim.inv_m, prim.m);
    prim.r[0] = 10.0f;
    prim.r[1] = 1.0f;
    _prims.push_back(prim);

    // left mountain
    prim.type = 1;
    make_rotation_matrix_2x2(prim.m, M_PI / 4.0f);
    prim.m[4] = 0.3f;
    prim.m[5] = 0.0f;
    orthonormal_invert_2x3(prim.inv_m, prim.m);
    prim.r[0] = 0.2f;
    prim.r[1] = 0.3f;
    _prims.push_back(prim);

    // right mountain
    prim.type = 1;
    make_rotation_matrix_2x2(prim.m, M_PI / 4.0f);
    prim.m[4] = 0.7f;
    prim.m[5] = 0.0f;
    orthonormal_invert_2x3(prim.inv_m, prim.m);
    prim.r[0] = 0.3f;
    prim.r[1] = 0.3f;
    _prims.push_back(prim);

    // house foundation
    prim.type = 1;
    make_rotation_matrix_2x2(prim.m, 0.0f);
    prim.m[4] = 0.6f;
    prim.m[5] = 0.4f;
    orthonormal_invert_2x3(prim.inv_m, prim.m);
    prim.r[0] = 0.2f;
    prim.r[1] = 0.03f;
    _prims.push_back(prim);

    // house left wall bottom
    prim.type = 1;
    make_rotation_matrix_2x2(prim.m, M_PI / 2.0f);
    prim.m[4] = 0.45f;
    prim.m[5] = 0.43f;
    orthonormal_invert_2x3(prim.inv_m, prim.m);
    prim.r[0] = 0.02f;
    prim.r[1] = 0.01f;
    _prims.push_back(prim);

    // house left wall top
    prim.type = 1;
    make_rotation_matrix_2x2(prim.m, M_PI / 2.0f);
    prim.m[4] = 0.45f;
    prim.m[5] = 0.545f;
    orthonormal_invert_2x3(prim.inv_m, prim.m);
    prim.r[0] = 0.05f;
    prim.r[1] = 0.01f;
    _prims.push_back(prim);

    // house right wall
    prim.type = 1;
    make_rotation_matrix_2x2(prim.m, M_PI / 2.0f);
    prim.m[4] = 0.75f;
    prim.m[5] = 0.5f;
    orthonormal_invert_2x3(prim.inv_m, prim.m);
    prim.r[0] = 0.1f;
    prim.r[1] = 0.01f;
    _prims.push_back(prim);

    // house roof left
    prim.type = 1;
    make_rotation_matrix_2x2(prim.m, M_PI / 6.0f);
    prim.m[4] = 0.48f;
    prim.m[5] = 0.6f;
    orthonormal_invert_2x3(prim.inv_m, prim.m);
    prim.r[0] = 0.15f;
    prim.r[1] = 0.02f;
    _prims.push_back(prim);

    // house roof right
    prim.type = 1;
    make_rotation_matrix_2x2(prim.m, -M_PI / 6.0f);
    prim.m[4] = 0.72f;
    prim.m[5] = 0.6f;
    orthonormal_invert_2x3(prim.inv_m, prim.m);
    prim.r[0] = 0.15f;
    prim.r[1] = 0.02f;
    _prims.push_back(prim);

    // tree trunk
    prim.type = 1;
    make_rotation_matrix_2x2(prim.m, 0.0f);
    prim.m[4] = 0.15f;
    prim.m[5] = 0.3f;
    orthonormal_invert_2x3(prim.inv_m, prim.m);
    prim.r[0] = 0.03f;
    prim.r[1] = 0.2f;
    _prims.push_back(prim);

    // tree bush
    prim.type = 0;
    make_rotation_matrix_2x2(prim.m, 0.0f);
    prim.m[4] = 0.15f;
    prim.m[5] = 0.5f;
    orthonormal_invert_2x3(prim.inv_m, prim.m);
    prim.r[0] = 0.11f;
    _prims.push_back(prim);

    // tree bush
    prim.type = 0;
    make_rotation_matrix_2x2(prim.m, 0.0f);
    prim.m[4] = 0.15f;
    prim.m[5] = 0.61f;
    orthonormal_invert_2x3(prim.inv_m, prim.m);
    prim.r[0] = 0.09f;
    _prims.push_back(prim);

    draw_sdf_prims(_prims, _width, _height, _buffer);

    // convert _buffer to _textureBuffer
    _textureBuffer = new uint8_t[_width * _height];
    int x, y;
    for (y = 0; y < _height; y++) {
        for (x = 0; x < _width; x++) {
            float *pixel = _buffer + (y * _width + x);
            uint8_t *texel = _textureBuffer + (y * _width + x);
            *texel = (uint8_t)(*pixel * 255.0f);
        }
    }
}

SDFScene::~SDFScene() {
    // TODO: use a unique_ptr
    delete [] _buffer;
    delete [] _textureBuffer;
}
