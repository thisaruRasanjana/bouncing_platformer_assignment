/*
 * 3D Bouncing Ball Platformer  -  Extended CG Demo
 * --------------------------------------------------------------
 * Original 5 concepts (unchanged):
 *   Member 1 - Camera & Viewing        : setupProjection(), setupCamera()
 *   Member 2 - Geometric Transformer   : drawPlatforms(), drawGround(), drawUnitCube()
 *   Member 3 - Animator                : updateBall() [keyframe], drawBall()
 *   Member 4 - Illumination & Materials: setupLighting(), applyGlossyMaterial(),
 *                                        applyMatteMaterial(), back-face culling
 *   Member 5 - Rendering & Depth       : Z-buffer + fog in initGL() / display()
 *
 * Added concepts - all woven into the ball animation as screen-space effects:
 *   Member 1 - DDA Line drawing        : radial spokes that burst from the ball on impact
 *   Member 1 - Window-to-Viewport      : drawMiniMap() (top-right corner)
 *   Member 2 - 2D Transforms           : orbiting diamond using translate/rotate/scale
 *   Member 2 - Polygon Fill            : hexagonal shadow beneath the ball
 *   Member 3 - Keyframe + Tweening     : updateBall() rewritten with KeyFrame structs
 *   Member 3 - Midpoint Circle         : expanding shockwave rings at each bounce impact
 *   Member 4 - Back-Face Culling       : glEnable(GL_CULL_FACE) in initGL()
 *   Member 5 - Cohen-Sutherland Clip   : 4 detection rays clipped to a zone around ball
 *
 * All 2D concept effects are anchored to the ball's projected screen position
 * (via gluProject), so they move with the ball and feel like part of the scene.
 *
 * Build (Linux):
 *   g++ main.cpp -o platformer -lGL -lGLU -lglut
 * Build (macOS):
 *   g++ main.cpp -o platformer -framework GLUT -framework OpenGL
 * Build (Windows, with freeglut):
 *   g++ main.cpp -o platformer.exe -lfreeglut -lopengl32 -lglu32
 *
 * Controls:
 *   ESC   - quit
 *   SPACE - pause / resume animation
 * --------------------------------------------------------------
 */

#ifdef __APPLE__
    #include <GLUT/glut.h>
#else
    #include <GL/glut.h>
#endif

#include <cmath>
#include <vector>

static const float PI = 3.14159265f;

// =================================================================
// Global State
// =================================================================

int windowWidth  = 1024;
int windowHeight = 768;

bool  paused     = false;
float globalTime = 0.0f;
const float TIME_STEP = 1.0f / 60.0f;

// Ball's projected screen position (filled each frame by gluProject)
float ballScreenX = 512.0f;
float ballScreenY = 384.0f;
float ballScreenZ = 0.5f;    // depth in [0,1] - used to skip off-screen ball
float ballScreenR = 40.0f;   // ball's projected radius in screen pixels

// ---------------- Member 3: Animator ----------------------------
struct Ball {
    float x, y, z;
    float radius;
    float bounceHeight;
    float bouncePeriod;
    float forwardSpeed;
    float scaleX, scaleY, scaleZ;
} ball;

// ---------------- Member 2: Geometric Transformer ---------------
struct Platform {
    float x, y, z;
    float width, height, depth;
    float rotationY;
    float r, g, b;
};
std::vector<Platform> platforms;

// ---------------- Member 1: Camera ------------------------------
struct Camera {
    float distanceBack;
    float heightAbove;
    float lookAheadZ;
} camera;

// ---------------- Member 4: Lighting ----------------------------
GLfloat lightPos[]      = { 5.0f, 15.0f, 5.0f, 0.0f }; // w=0 means directional light
GLfloat lightAmbient[]  = { 0.25f, 0.25f, 0.30f, 1.0f };
GLfloat lightDiffuse[]  = { 0.9f,  0.9f,  0.85f, 1.0f };
GLfloat lightSpecular[] = { 1.0f,  1.0f,  1.0f,  1.0f };

// ---------------- Member 5: Fog ---------------------------------
GLfloat fogColor[]  = { 0.55f, 0.65f, 0.80f, 1.0f };
float   fogDensity  = 0.028f;

// =================================================================
// Forward Declarations
// =================================================================
void initGL();
void initScene();
void setupLighting();
void setupFog();
void setupProjection(int w, int h);
void setupCamera();
void drawGround();
void drawPlatforms();
void updateBall(float dt);
void drawBall();
void projectBallPosition();
void draw2DOverlay();
void drawMiniMap();
void display();
void reshape(int w, int h);
void update(int value);
void keyboard(unsigned char key, int x, int y);

// 2D helpers
void enter2DMode();
void exit2DMode();
void drawDDALine(int x1, int y1, int x2, int y2);
void drawMidpointCircle(int cx, int cy, int r);

// Cohen-Sutherland
typedef int OutCode;
const int CS_INSIDE = 0, CS_LEFT = 1, CS_RIGHT = 2, CS_BOTTOM = 4, CS_TOP = 8;
OutCode computeOutCode(float x, float y, float xmin, float ymin, float xmax, float ymax);
bool    cohenSutherlandClip(float& x0, float& y0, float& x1, float& y1,
                            float xmin, float ymin, float xmax, float ymax);

// =================================================================
// MEMBER 1: Camera & Viewing Specialist
// =================================================================

// CONCEPT: Perspective Projection
void setupProjection(int w, int h)
{
    if (h == 0) h = 1;
    float aspect = (float)w / (float)h;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, aspect, 0.1, 200.0);
    glMatrixMode(GL_MODELVIEW);
}

// CONCEPT: Viewing Transformation (gluLookAt)
void setupCamera()
{
    glLoadIdentity();
    gluLookAt(ball.x,                        ball.y + camera.heightAbove,  ball.z + camera.distanceBack,
              ball.x,                        ball.y + 1.0f,                ball.z - camera.lookAheadZ,
              0.0f,                           1.0f,                         0.0f);
}

// Enter / exit 2D orthographic overlay mode
void enter2DMode()
{
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, glutGet(GLUT_WINDOW_WIDTH), 0, glutGet(GLUT_WINDOW_HEIGHT));
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
}

void exit2DMode()
{
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW);  glPopMatrix();
}

// CONCEPT: DDA (Digital Differential Analyzer) Line Drawing
// Rasterises a line by stepping by fractional dx/dy increments.
void drawDDALine(int x1, int y1, int x2, int y2)
{
    int   dx    = x2 - x1, dy = y2 - y1;
    int   steps = (abs(dx) > abs(dy)) ? abs(dx) : abs(dy);
    if   (steps == 0) return;
    float xInc  = dx / (float)steps;
    float yInc  = dy / (float)steps;
    float x     = (float)x1, y = (float)y1;
    glBegin(GL_POINTS);
    for (int i = 0; i <= steps; ++i) {
        glVertex2i((int)(x + 0.5f), (int)(y + 0.5f));
        x += xInc; y += yInc;
    }
    glEnd();
}

// CONCEPT: Window-to-Viewport Mapping  (Mini-Map, top-right corner)
// glViewport + gluOrtho2D perform the window-to-viewport transform directly.
void drawMiniMap()
{
    int W = glutGet(GLUT_WINDOW_WIDTH);
    int H = glutGet(GLUT_WINDOW_HEIGHT);
    const int VP = 120, MAR = 10;
    glViewport(W - VP - MAR, H - VP - MAR, VP, VP);

    float mapBottom = ball.z - 170.0f;
    float mapTop    = ball.z + 40.0f;

    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    gluOrtho2D(-10.0, 10.0, mapBottom, mapTop);
    glMatrixMode(GL_MODELVIEW);  glPushMatrix(); glLoadIdentity();

    glDisable(GL_LIGHTING); glDisable(GL_DEPTH_TEST); glDisable(GL_FOG);

    glColor3f(0.08f, 0.10f, 0.15f);
    glRectf(-10.f, mapBottom, 10.f, mapTop);

    for (size_t i = 0; i < platforms.size(); ++i) {
        const Platform& p = platforms[i];
        glColor3f(p.r, p.g, p.b);
        glRectf(p.x - p.width*0.4f, p.z - p.depth*0.4f,
                p.x + p.width*0.4f, p.z + p.depth*0.4f);
    }
    glColor3f(1.f, 0.2f, 0.2f);
    glRectf(ball.x - 0.8f, ball.z - 0.8f, ball.x + 0.8f, ball.z + 0.8f);

    glColor3f(0.55f, 0.35f, 0.75f);
    glBegin(GL_LINE_LOOP);
        glVertex2f(-10.f, mapBottom); glVertex2f(10.f, mapBottom);
        glVertex2f( 10.f, mapTop);    glVertex2f(-10.f, mapTop);
    glEnd();

    glEnable(GL_LIGHTING); glEnable(GL_DEPTH_TEST); glEnable(GL_FOG);

    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW);  glPopMatrix();
    glViewport(0, 0, W, H);
}

// =================================================================
// MEMBER 4: Illumination & Materials Engineer
// =================================================================

void setupLighting()
{
    glEnable(GL_LIGHTING); glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_AMBIENT,  lightAmbient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  lightDiffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecular);
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    GLfloat ga[] = { 0.15f, 0.15f, 0.18f, 1.0f };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ga);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_NORMALIZE);
}

// Glossy material – used for the ball
void applyGlossyMaterial(float r, float g, float b)
{
    glColor3f(r, g, b);
    GLfloat sp[] = { 0.9f, 0.9f, 0.9f, 1.0f }; GLfloat sh[] = { 64.0f };
    glMaterialfv(GL_FRONT, GL_SPECULAR, sp); glMaterialfv(GL_FRONT, GL_SHININESS, sh);
}

// Matte material – used for platforms / ground
void applyMatteMaterial(float r, float g, float b)
{
    glColor3f(r, g, b);
    GLfloat sp[] = { 0.05f, 0.05f, 0.05f, 1.0f }; GLfloat sh[] = { 4.0f };
    glMaterialfv(GL_FRONT, GL_SPECULAR, sp); glMaterialfv(GL_FRONT, GL_SHININESS, sh);
}

// =================================================================
// MEMBER 5: Rendering & Depth Specialist
// =================================================================

// CONCEPT: Exponential Fog
void setupFog()
{
    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_EXP2);
    glFogfv(GL_FOG_COLOR, fogColor);
    glFogf(GL_FOG_DENSITY, fogDensity);
    glHint(GL_FOG_HINT, GL_NICEST);
}

// CONCEPT: Cohen-Sutherland Line Clipping
OutCode computeOutCode(float x, float y, float xmin, float ymin, float xmax, float ymax)
{
    OutCode c = CS_INSIDE;
    if      (x < xmin) c |= CS_LEFT;
    else if (x > xmax) c |= CS_RIGHT;
    if      (y < ymin) c |= CS_BOTTOM;
    else if (y > ymax) c |= CS_TOP;
    return c;
}

bool cohenSutherlandClip(float& x0, float& y0, float& x1, float& y1,
                         float xmin, float ymin, float xmax, float ymax)
{
    OutCode o0 = computeOutCode(x0,y0,xmin,ymin,xmax,ymax);
    OutCode o1 = computeOutCode(x1,y1,xmin,ymin,xmax,ymax);
    while (true) {
        if (!(o0|o1)) return true;
        if ( (o0&o1)) return false;
        OutCode oc = o0 ? o0 : o1;
        float dx = x1-x0, dy = y1-y0, x, y;
        if      (oc & CS_TOP)    { x = x0 + dx*(ymax-y0)/dy; y = ymax; }
        else if (oc & CS_BOTTOM) { x = x0 + dx*(ymin-y0)/dy; y = ymin; }
        else if (oc & CS_RIGHT)  { y = y0 + dy*(xmax-x0)/dx; x = xmax; }
        else                     { y = y0 + dy*(xmin-x0)/dx; x = xmin; }
        if (oc == o0) { x0=x; y0=y; o0=computeOutCode(x0,y0,xmin,ymin,xmax,ymax); }
        else          { x1=x; y1=y; o1=computeOutCode(x1,y1,xmin,ymin,xmax,ymax); }
    }
}

// =================================================================
// Scene Initialisation
// =================================================================

void initGL()
{
    // CONCEPT: Z-Buffer
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL);
    glShadeModel(GL_SMOOTH);
    glClearColor(fogColor[0], fogColor[1], fogColor[2], fogColor[3]);
    setupLighting();
    setupFog();
    // CONCEPT: Back-Face Culling
    glEnable(GL_CULL_FACE); glCullFace(GL_BACK);
    // CONCEPT: Double Buffering – requested via GLUT_DOUBLE in main()
}

void initScene()
{
    ball.x = 0.f; ball.y = 1.f; ball.z = 0.f;
    ball.radius = 1.f; ball.bounceHeight = 1.5f;
    // bouncePeriod=1s × forwardSpeed=4 = 4 Z-units per bounce.
    // Platform spacing is also 4 Z-units, so every bounce lands on a platform.
    ball.bouncePeriod = 1.f; ball.forwardSpeed = 4.f;
    ball.scaleX = ball.scaleY = ball.scaleZ = 1.f;

    camera.distanceBack = 8.f; camera.heightAbove = 4.f; camera.lookAheadZ = 6.f;

    platforms.clear();
    // 40 platforms, 4 Z-units apart (= 1 bounce distance).
    // Depth reduced to 3 so adjacent platforms don't overlap.
    for (int i = 0; i < 40; ++i) {
        Platform p;
        p.width=5.5f; p.height=1.f; p.depth=3.0f;
        float xs[] = {0.f, 3.f, -3.f, 2.f, -2.f};
        p.x = xs[i%5]; p.y = -1.f; p.z = -4.f + i*(-4.f);
        p.rotationY = (i%3==0) ? 8.f : (i%3==1 ? -8.f : 0.f);
        if (i%2==0) { p.r=0.55f; p.g=0.35f; p.b=0.75f; }
        else        { p.r=0.35f; p.g=0.65f; p.b=0.55f; }
        platforms.push_back(p);
    }
}

// =================================================================
// MEMBER 2: Geometric Transformer
// =================================================================

void drawUnitCube()
{
    glBegin(GL_QUADS);
        glNormal3f( 0, 0, 1);
        glVertex3f(-0.5f,-0.5f, 0.5f); glVertex3f( 0.5f,-0.5f, 0.5f);
        glVertex3f( 0.5f, 0.5f, 0.5f); glVertex3f(-0.5f, 0.5f, 0.5f);
        glNormal3f( 0, 0,-1);
        glVertex3f( 0.5f,-0.5f,-0.5f); glVertex3f(-0.5f,-0.5f,-0.5f);
        glVertex3f(-0.5f, 0.5f,-0.5f); glVertex3f( 0.5f, 0.5f,-0.5f);
        glNormal3f(-1, 0, 0);
        glVertex3f(-0.5f,-0.5f,-0.5f); glVertex3f(-0.5f,-0.5f, 0.5f);
        glVertex3f(-0.5f, 0.5f, 0.5f); glVertex3f(-0.5f, 0.5f,-0.5f);
        glNormal3f( 1, 0, 0);
        glVertex3f( 0.5f,-0.5f, 0.5f); glVertex3f( 0.5f,-0.5f,-0.5f);
        glVertex3f( 0.5f, 0.5f,-0.5f); glVertex3f( 0.5f, 0.5f, 0.5f);
        glNormal3f( 0, 1, 0);
        glVertex3f(-0.5f, 0.5f, 0.5f); glVertex3f( 0.5f, 0.5f, 0.5f);
        glVertex3f( 0.5f, 0.5f,-0.5f); glVertex3f(-0.5f, 0.5f,-0.5f);
        glNormal3f( 0,-1, 0);
        glVertex3f(-0.5f,-0.5f,-0.5f); glVertex3f( 0.5f,-0.5f,-0.5f);
        glVertex3f( 0.5f,-0.5f, 0.5f); glVertex3f(-0.5f,-0.5f, 0.5f);
    glEnd();
}

void drawGround()
{
    glPushMatrix();
        applyMatteMaterial(0.30f, 0.30f, 0.35f);
        glTranslatef(0.f,-1.55f,-80.f);
        glScalef(40.f, 0.1f, 220.f);
        drawUnitCube();
    glPopMatrix();
}

void drawPlatforms()
{
    for (size_t i = 0; i < platforms.size(); ++i) {
        const Platform& p = platforms[i];
        glPushMatrix();
            applyMatteMaterial(p.r, p.g, p.b);
            glTranslatef(p.x, p.y, p.z);
            glRotatef(p.rotationY, 0.f, 1.f, 0.f);
            glScalef(p.width, p.height, p.depth);
            drawUnitCube();
        glPopMatrix();
    }
}

// =================================================================
// MEMBER 3: Animator
// =================================================================

// CONCEPT: Midpoint Circle Drawing Algorithm
void drawMidpointCircle(int cx, int cy, int r)
{
    if (r <= 0) return;
    int x = 0, y = r, p = 1 - r;
    glBegin(GL_POINTS);
    while (x <= y) {
        glVertex2i(cx+x,cy+y); glVertex2i(cx-x,cy+y);
        glVertex2i(cx+x,cy-y); glVertex2i(cx-x,cy-y);
        glVertex2i(cx+y,cy+x); glVertex2i(cx-y,cy+x);
        glVertex2i(cx+y,cy-x); glVertex2i(cx-y,cy-x);
        ++x;
        if (p < 0) { p += 2*x+1; }
        else       { --y; p += 2*x-2*y+1; }
    }
    glEnd();
}

// CONCEPT: Keyframe Animation + Linear Tweening
// Three keyframes: KF0 ground (squash), KF1 peak (stretch), KF2 ground (squash).
// Ball Y and scale values are LINEARLY INTERPOLATED between surrounding keyframes.
void updateBall(float dt)
{
    globalTime += dt;
    ball.z -= ball.forwardSpeed * dt;

    // Current phase in the bounce cycle (needed for both X-tracking and keyframes)
    float phase = fmodf(globalTime / ball.bouncePeriod, 1.0f);

    // ---- Smooth Platform X-tracking ------------------------------------
    // Instead of snapping to the target quickly, we compute a smooth transition 
    // using an ease-in-out curve based on the bounce phase. The ball glides 
    // sideways seamlessly over the exact duration of the jump.
    {
        float timeToLanding = (1.0f - phase) * ball.bouncePeriod;
        float landingZ      = ball.z - timeToLanding * ball.forwardSpeed;

        int targetIdx = -1;
        float minDist = 1.0e6f;
        for (size_t i = 0; i < platforms.size(); ++i) {
            float d = fabsf(platforms[i].z - landingZ);
            if (d < minDist) { minDist = d; targetIdx = (int)i; }
        }

        if (targetIdx >= 0) {
            float targetX = platforms[targetIdx].x;
            float prevX   = (targetIdx > 0) ? platforms[targetIdx - 1].x : 0.0f;

            // Ease-in-out interpolation: smooth start, fast middle, smooth stop
            float smoothT = 0.5f - 0.5f * cosf(phase * PI);
            ball.x = prevX + smoothT * (targetX - prevX);
        }
    }

    // ---- Keyframe animation --------------------------------------------
    struct KF { float t, yH, sY, sXZ; };
    const KF kf[] = {
        {0.0f, 0.0f, 0.65f, 1.35f},
        {0.5f, 1.0f, 1.20f, 0.94f},
        {1.0f, 0.0f, 0.65f, 1.35f},
    };
    float groundY = -0.5f + ball.radius;
    for (int i = 0; i < 2; ++i) {
        if (phase >= kf[i].t && phase <= kf[i+1].t) {
            float lt  = (phase - kf[i].t) / (kf[i+1].t - kf[i].t);
            ball.y      = groundY + (kf[i].yH  + lt*(kf[i+1].yH  - kf[i].yH))  * ball.bounceHeight;
            ball.scaleY = kf[i].sY  + lt*(kf[i+1].sY  - kf[i].sY);
            ball.scaleX = kf[i].sXZ + lt*(kf[i+1].sXZ - kf[i].sXZ);
            ball.scaleZ = ball.scaleX;
            break;
        }
    }
}

void drawBall()
{
    glPushMatrix();
        applyGlossyMaterial(0.95f, 0.25f, 0.25f);
        glTranslatef(ball.x, ball.y, ball.z);
        glScalef(ball.scaleX, ball.scaleY, ball.scaleZ);
        glutSolidSphere(ball.radius, 32, 32);
    glPopMatrix();
}

// =================================================================
// Animated screen-space effects  (all anchored to the ball)
// =================================================================

// Project the ball's 3D world position to 2D screen coordinates.
// Must be called after setupCamera() so the matrices are current.
void projectBallPosition()
{
    GLdouble mv[16], proj[16];
    GLint    vp[4];
    GLdouble wx, wy, wz;
    glGetDoublev(GL_MODELVIEW_MATRIX,  mv);
    glGetDoublev(GL_PROJECTION_MATRIX, proj);
    glGetIntegerv(GL_VIEWPORT,         vp);

    // Project ball centre
    gluProject(ball.x, ball.y, ball.z, mv, proj, vp, &wx, &wy, &wz);
    ballScreenX = (float)wx;
    ballScreenY = (float)wy;
    ballScreenZ = (float)wz;

    // Project a point on the ball's equator to derive the screen-space radius.
    // Using scaleX so the radius matches the squash/stretch state.
    GLdouble ex, ey, ez;
    gluProject(ball.x + ball.radius * ball.scaleX, ball.y, ball.z,
               mv, proj, vp, &ex, &ey, &ez);
    ballScreenR = fmaxf(fabsf((float)ex - ballScreenX), 18.0f);
}

void draw2DOverlay()
{
    if (ballScreenZ <= 0.0f || ballScreenZ >= 1.0f) return;

    float bx  = ballScreenX;
    float by  = ballScreenY;
    float bsr = ballScreenR;          // ball's on-screen radius
    float t   = globalTime;

    // Bounce phase: 0 = impact, 0.5 = peak, 1 = impact
    float phase    = fmodf(t / ball.bouncePeriod, 1.0f);
    float sinPhase = sinf(phase * PI);    // 0 at impact, 1 at peak
    float impact   = 1.0f - sinPhase;    // 1 at impact, 0 at peak

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_FOG);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    enter2DMode();

    // ----------------------------------------------------------------
    // CONCEPT: Polygon Fill  (scan-line fill via GL_POLYGON)
    // Multi-layer hexagonal shadow beneath the ball.
    // The shadow centre is pushed down from the ball by an amount
    // proportional to ball height (sinPhase), so at peak it is visibly
    // separated; at impact it sits right under the ball.
    // Three concentric hexagons create a soft-glow falloff.
    // ----------------------------------------------------------------
    {
        float sep       = bsr * 0.6f + 28.0f * sinPhase; // vertical gap ball<->shadow
        float shadowCY  = by - sep;
        float baseRx    = bsr * (0.8f + 0.5f * impact);
        float baseRy    = baseRx * 0.22f;                // flat on the ground

        for (int layer = 2; layer >= 0; --layer) {
            float scale  = 1.0f + layer * 0.35f;         // outer layers wider
            float alpha  = (0.12f + 0.30f * impact) * (1.0f - layer * 0.30f);
            glColor4f(0.06f, 0.03f, 0.16f, alpha);
            glBegin(GL_POLYGON);
            for (int i = 0; i < 6; ++i) {
                float a = i * 2.0f * PI / 6.0f;
                glVertex2f(bx         + baseRx * scale * cosf(a),
                           shadowCY   + baseRy * scale * sinf(a));
            }
            glEnd();
        }
    }

    // ----------------------------------------------------------------
    // CONCEPT: Midpoint Circle Drawing Algorithm
    // 3 shockwave rings expand outward from the ball's surface on each
    // impact.  Their start radius is bsr (the ball edge) so they never
    // overlap the sphere itself.  Point size 3.5 fills the gaps between
    // algorithm-computed pixels, giving a smooth ring appearance.
    // ----------------------------------------------------------------
    {
        glPointSize(3.5f);
        for (int ri = 0; ri < 3; ++ri) {
            float rPhase = fmodf(phase + (1.0f - ri * 0.15f), 1.0f);
            if (rPhase < 0.52f) {
                float prog  = rPhase / 0.52f;                       // 0=new, 1=gone
                // Ring starts at ball surface and expands outward
                int   ringR = (int)(bsr + prog * (52.0f + ri * 14.0f));
                float alpha = (1.0f - prog * prog) * (0.72f - ri * 0.14f);
                if (alpha <= 0.02f) continue;
                if (ri % 2 == 0) glColor4f(0.35f, 0.85f, 0.75f, alpha);
                else             glColor4f(0.65f, 0.50f, 0.90f, alpha);
                drawMidpointCircle((int)bx, (int)by, ringR);
            }
        }
        glPointSize(1.0f);
    }

    // ----------------------------------------------------------------
    // CONCEPT: DDA Line Drawing
    // 8 energy spokes radiate from the ball's surface (inner = bsr + gap)
    // outward.  They are longest and brightest at impact, short and faint
    // when the ball is airborne.  They rotate slowly for a living feel.
    // ----------------------------------------------------------------
    {
        float inner    = bsr + 4.0f;                         // just past ball edge
        float spokeLen = 10.0f + 26.0f * impact;
        float alpha    = 0.25f + 0.60f * impact;
        float rotation = t * 20.0f * (PI / 180.0f);          // slow spin

        glColor4f(0.72f, 0.52f, 0.95f, alpha);
        glPointSize(2.2f);
        for (int si = 0; si < 8; ++si) {
            float angle = rotation + si * (2.0f * PI / 8.0f);
            drawDDALine(
                (int)(bx + inner                  * cosf(angle)),
                (int)(by + inner                  * sinf(angle)),
                (int)(bx + (inner + spokeLen)     * cosf(angle)),
                (int)(by + (inner + spokeLen)     * sinf(angle))
            );
        }
        glPointSize(1.0f);
    }

    // ----------------------------------------------------------------
    // CONCEPT: 2D Translation / Rotation / Scaling
    // A red-hot diamond particle orbiting the ball at radius bsr+22.
    //   glTranslatef x2 -> to ball centre, then out to orbit radius
    //   glRotatef        -> drives the orbital + self-spin
    //   glScalef         -> pulsates with the bounce rhythm
    // A faint echo (lower alpha, slightly larger) gives a trail effect.
    // ----------------------------------------------------------------
    {
        float orbitR = bsr + 22.0f + 10.0f * sinPhase;

        // Draw a fading trail (3 ghost copies behind the main diamond)
        for (int ghost = 2; ghost >= 0; --ghost) {
            float ghostAngleOffset = ghost * 18.0f;   // degrees behind
            float ghostAlpha = (ghost == 0) ? 0.95f : (0.20f - ghost * 0.05f);
            float ghostScale = (ghost == 0) ? 1.0f  : (1.0f + ghost * 0.10f);

            glPushMatrix();
                glTranslatef(bx, by, 0.0f);
                glRotatef(t * 90.0f - ghostAngleOffset, 0.f, 0.f, 1.f);
                glTranslatef(orbitR, 0.0f, 0.0f);
                glRotatef(t * -155.0f, 0.f, 0.f, 1.f);
                float ps = 0.70f + 0.40f * sinf(t * 2.0f * PI / ball.bouncePeriod);
                glScalef(ps * ghostScale, ps * ghostScale, 1.0f);

                if (ghost == 0) {
                    glColor4f(0.95f, 0.30f, 0.30f, ghostAlpha);
                } else {
                    glColor4f(0.95f, 0.40f, 0.40f, ghostAlpha);
                }
                glBegin(GL_QUADS);
                    glVertex2f(-11.f, 0.f); glVertex2f( 0.f,-11.f);
                    glVertex2f( 11.f, 0.f); glVertex2f( 0.f, 11.f);
                glEnd();
                if (ghost == 0) {
                    glColor4f(1.f, 0.72f, 0.72f, 0.90f);
                    glBegin(GL_LINE_LOOP);
                        glVertex2f(-11.f, 0.f); glVertex2f( 0.f,-11.f);
                        glVertex2f( 11.f, 0.f); glVertex2f( 0.f, 11.f);
                    glEnd();
                }
            glPopMatrix();
        }
    }

    // ----------------------------------------------------------------
    // CONCEPT: Cohen-Sutherland Line Clipping
    // 4 cardinal sensor rays start from the ball's surface (bsr pixels
    // from centre) and extend 200px outward.  They are clipped to a zone
    // rectangle that is slightly larger than bsr, so the bright segment
    // is always fully outside the ball silhouette.  No zone border is
    // drawn – the visible rays look like clean energy beams emanating from
    // the ball surface, with a faint dim continuation beyond the zone.
    // ----------------------------------------------------------------
    {
        float zoneSize = bsr + 65.0f + 20.0f * sinPhase;
        float zxmin = bx - zoneSize, zymin = by - zoneSize * 0.7f;
        float zxmax = bx + zoneSize, zymax = by + zoneSize * 0.7f;
        float rayExt = 280.0f;

        // Cardinal directions: right, left, up, down
        float ex4[4] = {bx+rayExt, bx-rayExt, bx,       bx      };
        float ey4[4] = {by,        by,         by+rayExt,by-rayExt};

        for (int r = 0; r < 4; ++r) {
            float ex = ex4[r], ey = ey4[r];
            float dx = ex - bx, dy = ey - by;
            float len = sqrtf(dx*dx + dy*dy);
            // Start ray from ball surface, not from ball centre
            float sx = bx + (dx/len) * (bsr + 2.0f);
            float sy = by + (dy/len) * (bsr + 2.0f);

            // Full dim ray (shows the unclipped extent)
            glColor4f(0.45f, 0.40f, 0.65f, 0.12f);
            glBegin(GL_LINES); glVertex2f(sx, sy); glVertex2f(ex, ey); glEnd();

            // Clipped bright segment
            float cx0 = sx, cy0 = sy, cx1 = ex, cy1 = ey;
            if (cohenSutherlandClip(cx0, cy0, cx1, cy1, zxmin, zymin, zxmax, zymax)) {
                glColor4f(0.40f, 0.88f, 0.78f, 0.28f + 0.28f * sinPhase);
                glLineWidth(1.8f);
                glBegin(GL_LINES); glVertex2f(cx0, cy0); glVertex2f(cx1, cy1); glEnd();
                glLineWidth(1.0f);
            }
        }
    }

    exit2DMode();

    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_FOG);
}

// =================================================================
// GLUT Callbacks
// =================================================================

void display()
{
    // CONCEPT: Double Buffering
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    setupCamera();
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

    // Project ball to screen space (needed by draw2DOverlay)
    projectBallPosition();

    drawGround();
    drawPlatforms();
    drawBall();

    // All 2D concept effects centred on ball's screen position
    draw2DOverlay();

    // CONCEPT: Window-to-Viewport (Mini-Map) – top-right corner
    drawMiniMap();

    // CONCEPT: Double Buffering – swap front/back
    glutSwapBuffers();
}

void reshape(int w, int h)
{
    windowWidth = w; windowHeight = h;
    glViewport(0, 0, w, h);
    setupProjection(w, h);
}

void update(int value)
{
    if (!paused) {
        updateBall(TIME_STEP);
        
        // Infinite Track Logic: if the platform furthest behind is well behind the camera,
        // move it to the front of the track with the next valid sequence position.
        if (platforms.front().z > ball.z + camera.distanceBack + 5.0f) {
            Platform p = platforms.front();
            platforms.erase(platforms.begin());
            
            p.z = platforms.back().z - 4.0f;
            int idx = (int)(fabsf(p.z) / 4.0f) - 1;
            float xs[] = {0.f, 3.f, -3.f, 2.f, -2.f};
            p.x = xs[idx % 5];
            p.rotationY = (idx % 3 == 0) ? 8.f : (idx % 3 == 1 ? -8.f : 0.f);
            if (idx % 2 == 0) { p.r=0.55f; p.g=0.35f; p.b=0.75f; }
            else              { p.r=0.35f; p.g=0.65f; p.b=0.55f; }
            
            platforms.push_back(p);
        }
    }
    glutPostRedisplay();
    glutTimerFunc((unsigned int)(TIME_STEP * 1000.0f), update, 0);
}

void keyboard(unsigned char key, int x, int y)
{
    switch (key) {
        case 27:  exit(0);           break;
        case ' ': paused = !paused;  break;
        default:                     break;
    }
}

// =================================================================
// Main
// =================================================================

int main(int argc, char** argv)
{
    glutInit(&argc, argv);
    // CONCEPT: Double Buffering
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(windowWidth, windowHeight);
    glutCreateWindow("3D Bouncing Ball Platformer - CG Demo");

    initGL();
    initScene();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutTimerFunc((unsigned int)(TIME_STEP * 1000.0f), update, 0);

    glutMainLoop();
    return 0;
}
