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

static const int BUFFER_SIZE = 512;
static const float SAMPLES_PER_METER = 128;
static const float MAX_DISTANCE = 1.0f;
static const float WORLD_SIZE = (float)BUFFER_SIZE / SAMPLES_PER_METER;

static const float WORLD_TO_BUFFER_SCALE = (float)BUFFER_SIZE / (float)WORLD_SIZE;
static glm::mat3 WORLD_TO_BUFFER_MAT(glm::vec3(WORLD_TO_BUFFER_SCALE, 0.0f, 0.0f),
                                     glm::vec3(0.0f, WORLD_TO_BUFFER_SCALE, 0.0f),
                                     glm::vec3((float)BUFFER_SIZE / 2.0f, (float)BUFFER_SIZE / 2.0f, 1.0f));
static glm::mat3 BUFFER_TO_WORLD_MAT = glm::inverse(WORLD_TO_BUFFER_MAT);

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

// polynomial smooth min (k = 0.1);
// https://www.iquilezles.org/www/articles/smin/smin.htm
float smin(float a, float b, float k = 0.1f) {
    float h = std::max(k - fabsf(a - b), 0.0f);
    return std::min(a, b) - h * h * 0.25f / k;
}

// https://www.iquilezles.org/www/articles/smin/smin.htm
float smax(float a, float b, float k = 0.1f)
{
    float h = std::max(k - fabsf(a - b), 0.0f);
    return std::max(a, b) + h * h * 0.25f / k;
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
    result.dist = std::min(MAX_DISTANCE, dist);
    result.nearest_prim = nearest_prim;
    return result;
}

static void draw_sdf_prims(const std::vector<Prim>& prims, int size, float* buffer) {
    int x, y;
    for (y = 0; y < size; y++) {
        for (x = 0; x < size; x++) {
            float *pixel = buffer + (y * size + x);

            // convert from "pixel" coordinates into "world" space
            glm::vec2 bufferPoint((float)x, (float)y);
            glm::vec2 worldPoint = BUFFER_TO_WORLD_MAT * glm::vec3(bufferPoint, 1.0f);

            MapResult r = map(prims, (float*)&worldPoint);
            float dist = r.dist;

            *pixel = dist;
        }
    }
}

static void add_sdf_prim(const Prim& prim, int size, float* buffer) {
    int x, y;
    for (y = 0; y < size; y++) {
        for (x = 0; x < size; x++) {
            float *pixel = buffer + (y * size + x);

            // convert from "pixel" coordinates into "world" space
            glm::vec2 bufferPoint((float)x, (float)y);
            glm::vec2 worldPoint = BUFFER_TO_WORLD_MAT * glm::vec3(bufferPoint, 1.0f);

            *pixel = smin(*pixel, sdf_prim((float*)&worldPoint, prim));
        }
    }
}

static void rem_sdf_prim(const Prim& prim, int size, float* buffer) {
    int x, y;
    for (y = 0; y < size; y++) {
        for (x = 0; x < size; x++) {
            float *pixel = buffer + (y * size + x);

            // convert from "pixel" coordinates into "world" space
            glm::vec2 bufferPoint((float)x, (float)y);
            glm::vec2 worldPoint = BUFFER_TO_WORLD_MAT * glm::vec3(bufferPoint, 1.0f);

            *pixel = smax(*pixel, -sdf_prim((float*)&worldPoint, prim));
        }
    }
}

SDFScene::SDFScene() {
    _size = BUFFER_SIZE;
    _buffer = new float[_size * _size];

    // ground
    Prim prim;

    // origin 1 meter sphere
    prim.type = 0;
    make_rotation_matrix_2x2(prim.m, 0.0f);
    prim.m[4] = 0.0f;
    prim.m[5] = 0.0f;
    orthonormal_invert_2x3(prim.inv_m, prim.m);
    prim.r[0] = 0.01f;
    prim.r[1] = 0.01f;
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

    draw_sdf_prims(_prims, _size, _buffer);

    // AJT: TEST CODE REMOVE

    printf("BUFFER_TO_WORLD_MAT =\n");
    printf("| %10.3f, %10.3f, %10.3f |\n", BUFFER_TO_WORLD_MAT[0].x, BUFFER_TO_WORLD_MAT[1].x, BUFFER_TO_WORLD_MAT[2].x);
    printf("| %10.3f, %10.3f, %10.3f |\n", BUFFER_TO_WORLD_MAT[0].y, BUFFER_TO_WORLD_MAT[1].y, BUFFER_TO_WORLD_MAT[2].y);
    printf("| %10.3f, %10.3f, %10.3f |\n", BUFFER_TO_WORLD_MAT[0].z, BUFFER_TO_WORLD_MAT[1].z, BUFFER_TO_WORLD_MAT[2].z);

    printf("WORLD_TO_BUFFER_MAT =\n");
    printf("| %10.3f, %10.3f, %10.3f |\n", WORLD_TO_BUFFER_MAT[0].x, WORLD_TO_BUFFER_MAT[1].x, WORLD_TO_BUFFER_MAT[2].x);
    printf("| %10.3f, %10.3f, %10.3f |\n", WORLD_TO_BUFFER_MAT[0].y, WORLD_TO_BUFFER_MAT[1].y, WORLD_TO_BUFFER_MAT[2].y);
    printf("| %10.3f, %10.3f, %10.3f |\n", WORLD_TO_BUFFER_MAT[0].z, WORLD_TO_BUFFER_MAT[1].z, WORLD_TO_BUFFER_MAT[2].z);

    glm::vec2 worldPoint(0.0f, 0.0f);
    glm::vec2 bufferPoint = WORLD_TO_BUFFER_MAT * glm::vec3(worldPoint, 1.0f);
    printf("AJT: worldPoint = (%.5f, %.5f)\n", worldPoint.x, worldPoint.y);
    printf("AJT: bufferPoint = (%.5f, %.5f)\n", bufferPoint.x, bufferPoint.y);
    worldPoint = BUFFER_TO_WORLD_MAT * glm::vec3(bufferPoint, 1.0f);
    printf("AJT: worldPoint = (%.5f, %.5f)\n", worldPoint.x, worldPoint.y);

    // Create a cresent moon
    AddCircle(glm::vec2(2.0f, 2.0f), 0.5f);
    RemCircle(glm::vec2(2.2f, 2.2f), 0.5f);
}

SDFScene::~SDFScene() {
    // TODO: use a unique_ptr
    delete [] _buffer;
}

void SDFScene::AddCircle(const glm::vec2& pos, float radius) {
    Prim prim;
    prim.type = 0;
    make_rotation_matrix_2x2(prim.m, 0.0f);
    prim.m[4] = pos.x;
    prim.m[5] = pos.y;
    orthonormal_invert_2x3(prim.inv_m, prim.m);
    prim.r[0] = radius;

    add_sdf_prim(prim, _size, _buffer);
}

void SDFScene::RemCircle(const glm::vec2& pos, float radius) {
    Prim prim;
    prim.type = 0;
    make_rotation_matrix_2x2(prim.m, 0.0f);
    prim.m[4] = pos.x;
    prim.m[5] = pos.y;
    orthonormal_invert_2x3(prim.inv_m, prim.m);
    prim.r[0] = radius;

    rem_sdf_prim(prim, _size, _buffer);
}

int SDFScene::GetSamplesPerMeter() const {
    return SAMPLES_PER_METER;
}
