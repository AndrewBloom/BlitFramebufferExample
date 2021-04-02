/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// OpenGL ES 2.0 code

#include <jni.h>
#include <android/log.h>

#include <GLES3/gl32.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define  LOG_TAG    "libgl2jni"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

static void printGLString(const char *name, GLenum s) {
    const char *v = (const char *) glGetString(s);
    LOGI("GL %s = %s\n", name, v);
}

static void checkGlError(const char* op) {
    for (GLint error = glGetError(); error; error
            = glGetError()) {
        LOGI("after %s() glError (0x%x)\n", op, error);
    }
}

static const char* kHeader = R"(#version 310 es
)";

static const char* kQuadVS = R"(
#extension GL_EXT_shader_io_blocks : enable

precision highp float;

layout(location = 0) in vec4 vPosition;
layout(location = 1) in vec2 texCoord;

out vec2 v_texCoord;

void main() {
  gl_Position = vPosition;
  v_texCoord = texCoord;
}
)";

static const char* kQuadFS = R"(
precision highp float;

layout(location = 0) uniform sampler2D uTexture0;
in vec2 v_texCoord;
layout(location = 0) out vec4 fsColor;

void main() {
  fsColor = texture(uTexture0, v_texCoord);
}
)";

static constexpr uint32_t kTexture0Uniform = 0;
static constexpr uint32_t kVerticesArrayAttr = 0;
static constexpr uint32_t kUVsArrayAttr = 1;

GLuint createProgram() {
    GLuint VS = glCreateShader(GL_VERTEX_SHADER);
    {
        const GLint sizes[] = { static_cast<GLint>(strlen(kHeader)),
                                static_cast<GLint>(strlen(kQuadVS)) };
        const char* lines[] = { kHeader, kQuadVS };
        glShaderSource(VS, 2, lines, sizes);
        glCompileShader(VS);
    }
    GLuint FS = glCreateShader(GL_FRAGMENT_SHADER);
    {
        const GLint sizes[] = { static_cast<GLint>(strlen(kHeader)),
                                static_cast<GLint>(strlen(kQuadFS)) };
        const char* lines[] = { kHeader, kQuadFS };
        glShaderSource(FS, 2, lines, sizes);
        glCompileShader(FS);
    }

    LOGI("Compiled shaders");
    GLuint program = glCreateProgram();

    glAttachShader(program, VS);
    glAttachShader(program, FS);

    glLinkProgram(program);

    GLint status = GL_TRUE;
    glGetProgramiv(program, GL_LINK_STATUS, &status);

    if (GL_FALSE == status)
    {
        char str[1024];
        glGetProgramInfoLog(program, sizeof(str), NULL, str);
        LOGE("Program linking failed:\n%s", str);

        glDeleteProgram(program);
        program = 0;
    }

    glDeleteShader(VS);
    glDeleteShader(FS);
    LOGI("Compiled program");

    return program;
}

GLuint gProgram;
GLuint tex;

void createTexture(GLuint &tex, int width, int height, int r){
    int rowlen = width * 3;
    unsigned char buf[width*3*height];
    for (int i=0; i<rowlen; i+=3)
        for (int j=0; j<height; j++) {
            int col = ((int (i/(3*r))) + (int (j/r)))%2;
            buf[i+j*rowlen] = buf[i+1+j*rowlen] = buf[i+2+j*rowlen] = col * 255;
        }
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, buf);
    // set the texture wrapping/filtering options (on the currently bound texture object)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    LOGI("DEBUG C++ Texture created [%d]", tex);
}

GLuint fb;
GLuint aniTex[25];
GLuint fbr = 0;

bool SetupFramebuffer(GLuint tex, int width, int height)
{
    if (fb == 0)
    {
        glGenFramebuffers(1, &fb);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, tex, 0);

        GLenum status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            LOGE("Incomplete frame buffer object!");
            return false;
        }

        LOGI("Created FBO %d for texture %d.",
             fb, tex);
    }

    return true;
}

constexpr int tw = 240;
constexpr int th = 240;
int width, height;

bool setupGraphics(int w, int h) {
    printGLString("Version", GL_VERSION);
    printGLString("Vendor", GL_VENDOR);
    printGLString("Renderer", GL_RENDERER);
    printGLString("Extensions", GL_EXTENSIONS);
    width = w;
    height = h;

    LOGI("setupGraphics(%d, %d)", w, h);
    gProgram = createProgram();
    if (!gProgram) {
        LOGE("Could not create program.");
        return false;
    }

    glViewport(0, 0, w, h);
    checkGlError("glViewport");
    createTexture(tex, tw, th, 6);
    for (int i = 0; i<25; i++) createTexture(aniTex[i], tw, th, 3+i);
    SetupFramebuffer(tex, tw, th);

    return true;
}

const GLfloat gTriangleVertices[] = { 0.0f, 0.5f, -0.5f, -0.5f,
        0.5f, -0.5f };
const GLfloat gTriangleUVs[] = { 0.5f, 1.0f, 0.0f, 0.0f,
                                      1.0f, 0.0f };
const GLenum attachment[1] = { GL_COLOR_ATTACHMENT0 };

void blitTexture() {
    static int time = 0;
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    checkGlError("blitTexture glBindFramebuffer");

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
                           GL_TEXTURE_2D, aniTex[time], 0);
    checkGlError("blitTexture glFramebufferTexture2D");
    glReadBuffer(GL_COLOR_ATTACHMENT1);
    checkGlError("blitTexture glReadBuffer");
    glDrawBuffers(1, attachment);
    checkGlError("blitTexture glDrawBuffers");


    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) LOGE("Incomplete frame buffer object!");

    glBlitFramebuffer(0,0,tw, th, 0, 0, tw, th, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    int samples;
    glGetFramebufferParameteriv(GL_READ_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_SAMPLES, &samples);
    LOGI("SAMPLES draw buffer %d", samples);
    checkGlError("blitTexture glBlitFramebuffer");
    time = ++time%25;
}

void renderFrame() {
    static float grey;
    grey += 0.001f;
    if (grey > 1.0f) {
        grey = 0.0f;
    }
    glClearColor(grey, 0.0f, 0.0f, 1.0f);

    blitTexture();
    checkGlError("blitTexture");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glScissor(0, 0, width, height);



    checkGlError("glClearColor");
    glClear( GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    checkGlError("glClear");

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    checkGlError("glBindTexture");
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glUniform1i(kTexture0Uniform, 0);

    checkGlError("glUniform1i");

    glUseProgram(gProgram);
    checkGlError("glUseProgram");

    glVertexAttribPointer(kVerticesArrayAttr, 2, GL_FLOAT, GL_FALSE, 0, gTriangleVertices);
    checkGlError("glVertexAttribPointer");
    glEnableVertexAttribArray(kVerticesArrayAttr);
    glVertexAttribPointer(kUVsArrayAttr, 2, GL_FLOAT, GL_FALSE, 0, gTriangleUVs);
    checkGlError("glVertexAttribPointer2");
    glEnableVertexAttribArray(kUVsArrayAttr);
    checkGlError("glEnableVertexAttribArray");
    glDrawArrays(GL_TRIANGLES, 0, 3);
    checkGlError("glDrawArrays");
}

extern "C" {
    JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_init(JNIEnv * env, jobject obj,  jint width, jint height);
    JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_step(JNIEnv * env, jobject obj);
};

JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_init(JNIEnv * env, jobject obj,  jint width, jint height)
{
    setupGraphics(width, height);
}

JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_step(JNIEnv * env, jobject obj)
{
    renderFrame();
}
