#ifndef SHADERS_H
#define SHADERS_H

// Vertex shader generates a screen-spanning triangle automatically using gl_VertexID.
// This allows rendering a fullscreen quad with zero buffer bindings.
const char* vertexShaderSource = R"(
#version 130
out vec2 uv;
void main() {
    uint id = uint(gl_VertexID);
    float x = float((id & 1u) << 2u) - 1.0;
    float y = float((id & 2u) << 1u) - 1.0;
    uv = vec2(x, y) * 0.5 + 0.5;
    gl_Position = vec4(x, y, 0.0, 1.0);
}
)";

// Fragment shader: high-performance raymarcher with adaptive AA and 5 beautiful dynamically parameterized fractals.
const char* fragmentShaderSource = R"(
#version 130

in vec2 uv;
out vec4 fragColor;

uniform float u_time;
uniform vec2 u_resolution;
uniform int u_fractalType; // 0=Mandelbulb, 1=Julia, 2=Jerusalem Cube, 3=Sierpinski Pyramid, 4=Menger Sponge
uniform int u_useAA;       // 0=Disabled, 1=Enabled
uniform int u_maxSteps;    // Dynamic raymarch step count limit for hardware profiling

uniform vec3 u_cameraPos;
uniform vec3 u_cameraLookAt;

// Cosine gradient palette parameters
uniform vec3 u_paletteA;
uniform vec3 u_paletteB;
uniform vec3 u_paletteC;
uniform vec3 u_paletteD;
uniform vec3 u_bgColor;

// Dynamic fractal iteration and parameter variables
uniform int u_iterMandelbulb;
uniform float u_mandelbulbPower;

uniform int u_iterJulia;
uniform vec4 u_juliaC;

uniform int u_iterJerusalem;
uniform float u_jerusalemScale;

uniform int u_iterSierpinski;
uniform float u_sierpinskiScale;

uniform int u_iterMenger;
uniform float u_mengerScale;

// ponytail: configurable camera proximity threshold constants to keep camera in clear zones
#define CAM_MIN_DIST 0.08
#define CAM_COLLISION_DAMP 0.02

// 1. Mandelbulb DE
float MandelbulbDE(vec3 p, out float orbit) {
    vec3 z = p;
    float dr = 1.0;
    float r = 0.0;
    orbit = 0.0;
    for (int i = 0; i < u_iterMandelbulb; i++) {
        r = length(z);
        if (r > 2.0) break;
        
        float theta = acos(z.z / r);
        float phi = atan(z.y, z.x);
        dr = pow(r, u_mandelbulbPower - 1.0) * u_mandelbulbPower * dr + 1.0;
        
        float zr = pow(r, u_mandelbulbPower);
        theta = theta * u_mandelbulbPower;
        phi = phi * u_mandelbulbPower;
        
        z = zr * vec3(sin(theta) * cos(phi), sin(phi) * sin(theta), cos(theta));
        z += p;
        orbit += dot(z, z);
    }
    return 0.5 * log(r) * r / dr;
}

// 2. Quaternion Julia DE
vec4 qSq(vec4 q) {
    return vec4(
        q.x * q.x - dot(q.yzw, q.yzw),
        2.0 * q.x * q.yzw
    );
}

float JuliaDE(vec3 p, out float orbit) {
    vec4 z = vec4(p, 0.0);
    float dr2 = 1.0;
    float r2 = 0.0;
    orbit = 0.0;
    for (int i = 0; i < u_iterJulia; i++) {
        dr2 = 4.0 * r2 * dr2 + 1.0;
        z = qSq(z) + u_juliaC;
        r2 = dot(z, z);
        if (r2 > 4.0) break;
        orbit += r2;
    }
    return 0.5 * sqrt(r2 / dr2) * log(sqrt(r2));
}

// 3. Jerusalem Cube DE
float JerusalemCubeDE(vec3 p, out float orbit) {
    float d = max(abs(p.x), max(abs(p.y), abs(p.z))) - 1.0;
    float s = 1.0;
    orbit = 0.0;
    for (int i = 0; i < u_iterJerusalem; i++) {
        vec3 a = mod(p * s + 1.0, 2.0) - 1.0;
        s *= u_jerusalemScale;
        vec3 r = abs(1.0 - u_jerusalemScale * abs(a));
        float da = max(r.x, r.y);
        float db = max(r.y, r.z);
        float dc = max(r.z, r.x);
        float c = (min(da, min(db, dc)) - 1.0) / s;
        d = max(d, c);
        orbit += dot(a, a);
    }
    return d;
}

// 4. Sierpinski Pyramid DE
float SierpinskiPyramidDE(vec3 p, out float orbit) {
    const vec3 v0 = vec3(1.0, 1.0, 1.0);
    float s = 1.0;
    orbit = 0.0;
    for (int i = 0; i < u_iterSierpinski; i++) {
        if (p.x + p.y < 0.0) p.xy = -p.yx;
        if (p.x + p.z < 0.0) p.xz = -p.zx;
        if (p.y + p.z < 0.0) p.zy = -p.yz;
        p = p * u_sierpinskiScale - v0 * (u_sierpinskiScale - 1.0);
        s *= u_sierpinskiScale;
        orbit += dot(p, p);
    }
    // distance to flat faces of regular tetrahedron instead of spheres
    return (max(p.x + p.y + p.z, max(p.x - p.y - p.z, max(-p.x + p.y - p.z, -p.x - p.y + p.z))) - 1.0) / s;
}

// Helper box function for Menger Sponge
float sdBox(vec3 p, vec3 b) {
    vec3 q = abs(p) - b;
    return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0);
}

// 5. Menger Sponge DE (Drilling axis crosses)
float MengerSpongeDE(vec3 p, out float orbit) {
    float d = sdBox(p, vec3(1.0));
    float s = 1.0;
    orbit = 0.0;
    for (int m = 0; m < u_iterMenger; m++) {
        vec3 a = mod(p * s, 2.0) - 1.0;
        s *= u_mengerScale;
        vec3 r = abs(1.0 - u_mengerScale * abs(a));
        float da = max(r.x, r.y);
        float db = max(r.y, r.z);
        float dc = max(r.z, r.x);
        float c = (min(da, min(db, dc)) - 1.0) / s;
        if (c > d) {
            d = c;
        }
        orbit += dot(a, a);
    }
    return d;
}

// Distance estimator dispatcher
float getDistance(vec3 p, out float orbit) {
    if (u_fractalType == 0) return MandelbulbDE(p, orbit);
    if (u_fractalType == 1) return JuliaDE(p, orbit);
    if (u_fractalType == 2) return JerusalemCubeDE(p, orbit);
    if (u_fractalType == 3) return SierpinskiPyramidDE(p, orbit);
    return MengerSpongeDE(p, orbit);
}

// Estimate normal
vec3 getNormal(vec3 p) {
    vec2 e = vec2(0.0005, 0.0);
    float dummy;
    return normalize(vec3(
        getDistance(p + e.xyy, dummy) - getDistance(p - e.xyy, dummy),
        getDistance(p + e.yxy, dummy) - getDistance(p - e.yxy, dummy),
        getDistance(p + e.yyx, dummy) - getDistance(p - e.yyx, dummy)
    ));
}

// Soft shadows
float getShadow(vec3 ro, vec3 rd, float mint, float maxt) {
    float res = 1.0;
    float t = mint;
    float dummy;
    for (int i = 0; i < 16; i++) {
        float h = getDistance(ro + rd * t, dummy);
        if (h < 0.001) return 0.0;
        res = min(res, 8.0 * h / t);
        t += clamp(h, 0.01, 0.2);
        if (t > maxt) break;
    }
    return clamp(res, 0.0, 1.0);
}

// Cosine gradient evaluation
vec3 evaluatePalette(float t) {
    return u_paletteA + u_paletteB * cos(6.28318 * (u_paletteC * t + u_paletteD));
}

// Rendering formula
vec3 getRenderColor(vec3 ro, vec3 rd, float t, float orbit, bool hit, float maxDist) {
    vec3 col = u_bgColor;

    if (hit) {
        vec3 pos = ro + rd * t;
        vec3 nor = getNormal(pos);
        vec3 lig = normalize(vec3(sin(u_time * 0.2), 0.8, cos(u_time * 0.2)));
        
        float dif = clamp(dot(nor, lig), 0.0, 1.0);
        float spe = pow(clamp(dot(reflect(rd, nor), lig), 0.0, 1.0), 16.0);
        float ao = clamp(1.0 - (orbit * 0.01), 0.2, 1.0);
        float sha = getShadow(pos + nor * 0.01, lig, 0.01, 2.0);

        float colorCoord = orbit * 0.05 + u_time * 0.01;
        vec3 mate = evaluatePalette(colorCoord);

        col = mate * (dif * sha + 0.15) + vec3(0.8) * spe * sha;
        col *= ao;
        col = mix(col, u_bgColor, 1.0 - exp(-0.08 * t * t));
    } else {
        float bgGrad = 0.5 + 0.5 * rd.y;
        col = mix(u_bgColor, u_bgColor * 0.2, bgGrad);
        
        float starDust = fract(sin(dot(rd.xy, vec2(12.9898, 78.233))) * 43758.5453);
        if (starDust > 0.998) {
            col += vec3(0.4 * starDust);
        }
    }
    return col;
}

void main() {
    vec2 p = (gl_FragCoord.xy * 2.0 - u_resolution) / min(u_resolution.x, u_resolution.y);

    vec3 ro = u_cameraPos;
    vec3 ta = u_cameraLookAt;
    vec3 ww = normalize(ta - ro);

    // ponytail: enforce non-busy camera start/path by backing up along sight line (ww) if close to surfaces
    float dummyVal;
    float dCam = getDistance(ro, dummyVal);
    if (dCam < CAM_MIN_DIST) {
        ro -= ww * (CAM_MIN_DIST - dCam + CAM_COLLISION_DAMP);
    }

    vec3 uu = normalize(cross(ww, vec3(0.0, 1.0, 0.0)));
    vec3 vv = normalize(cross(uu, ww));
    vec3 rd = normalize(p.x * uu + p.y * vv + 1.5 * ww);

    float t = 0.0;
    float maxDist = (u_fractalType == 2 || u_fractalType == 4) ? 12.0 : 6.0;
    float threshold = 0.0005;
    float orbit = 0.0;
    bool hit = false;
    int steps = 0;
    
    for (int i = 0; i < u_maxSteps; i++) {
        steps++;
        vec3 pos = ro + rd * t;
        float d = getDistance(pos, orbit);
        if (d < threshold) {
            hit = true;
            break;
        }
        t += d;
        if (t > maxDist) break;
    }

    vec3 col = getRenderColor(ro, rd, t, orbit, hit, maxDist);

    // Adaptive lightweight antialiasing (toggled by u_useAA)
    if (u_useAA == 1 && steps > (u_maxSteps * 3 / 5)) {
        float orbit2 = 0.0;
        float t2 = 0.0;
        bool hit2 = false;
        
        vec2 offset = vec2(0.5, 0.5) / u_resolution;
        vec2 p2 = p + offset;
        vec3 rd2 = normalize(p2.x * uu + p2.y * vv + 1.5 * ww);
        
        for (int i = 0; i < u_maxSteps; i++) {
            vec3 pos = ro + rd2 * t2;
            float d = getDistance(pos, orbit2);
            if (d < threshold) {
                hit2 = true;
                break;
            }
            t2 += d;
            if (t2 > maxDist) break;
        }
        vec3 col2 = getRenderColor(ro, rd2, t2, orbit2, hit2, maxDist);
        col = (col + col2) * 0.5;
    }

    // Gamma correction
    col = pow(col, vec3(0.4545));

    fragColor = vec4(col, 1.0);
}
)";

#endif
