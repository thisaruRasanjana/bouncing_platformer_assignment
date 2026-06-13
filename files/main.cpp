/*
 * 3D Bouncing Ball Platformer
 * --------------------------------------------------------------
 * A demo OpenGL/FreeGLUT program that demonstrates 5 core
 * Computer Graphics concepts, each corresponding to one team
 * member's role in the project plan:
 *
 *  Member 1 - Camera & Viewing       : setupProjection(), setupCamera()
 *  Member 2 - Geometric Transformer  : drawPlatforms()
 *  Member 3 - Animator               : updateBall(), drawBall() (squash & stretch)
 *  Member 4 - Illumination & Materials: setupLighting(), material calls
 *  Member 5 - Rendering & Depth      : Z-buffer + fog setup in init()/display()
 *
 * Build (Linux):
 *   g++ main.cpp -o platformer -lGL -lGLU -lglut
 *
 * Build (Windows, with freeglut installed):
 *   g++ main.cpp -o platformer.exe -lfreeglut -lopengl32 -lglu32
 *
 * Controls:
 *   ESC   - quit
 *   SPACE - pause/resume animation
 * --------------------------------------------------------------
 */

#ifdef __APPLE__
    #include <GLUT/glut.h>
#else
    #include <GL/glut.h>
#endif

#include <cmath>
#include <vector>

// =================================================================
// Global State
// =================================================================

// Window
int windowWidth  = 1024;
int windowHeight = 768;

// Animation control
bool paused = false;
float globalTime = 0.0f;          // seconds
const float TIME_STEP = 1.0f / 60.0f; // fixed update for smooth periodic motion

// ---------------- Member 3: Animator state ----------------------
// Ball travels forward along -Z, bounces up/down (Y), and squashes
// on impact using a non-uniform scale matrix.
struct Ball {
    float x, y, z;         // position
    float radius;
    float bounceHeight;    // amplitude of vertical bounce
    float bouncePeriod;    // seconds per full bounce cycle
    float forwardSpeed;    // units per second along Z

    float scaleX, scaleY, scaleZ; // squash & stretch factors
} ball;

// ---------------- Member 2: Geometric Transformer ----------------
// A platform is a flat box positioned/rotated in the world.
struct Platform {
    float x, y, z;     // position (center)
    float width, height, depth;
    float rotationY;   // tilt around Y axis (degrees) for visual variety
    float r, g, b;     // color
};
std::vector<Platform> platforms;

// ---------------- Member 1: Camera --------------------------------
// Camera follows behind and slightly above the ball.
struct Camera {
    float distanceBack; // how far behind the ball
    float heightAbove;  // how far above the ball
    float lookAheadZ;   // how far ahead of the ball the camera looks
} camera;

// ---------------- Member 4: Lighting ------------------------------
GLfloat lightPos[]      = { 5.0f, 15.0f, 5.0f, 1.0f };  // positional "sun"
GLfloat lightAmbient[]  = { 0.25f, 0.25f, 0.30f, 1.0f };
GLfloat lightDiffuse[]  = { 0.9f, 0.9f, 0.85f, 1.0f };
GLfloat lightSpecular[] = { 1.0f, 1.0f, 1.0f, 1.0f };

// ---------------- Member 5: Fog -----------------------------------
GLfloat fogColor[] = { 0.55f, 0.65f, 0.80f, 1.0f }; // matches background
float fogDensity = 0.04f;

// =================================================================
// Forward declarations
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
void display();
void reshape(int w, int h);
void update(int value);
void keyboard(unsigned char key, int x, int y);

// =================================================================
// MEMBER 1: Camera & Viewing Specialist
// =================================================================

// Sets up the perspective projection matrix (field of view, aspect, near/far)
void setupProjection(int w, int h)
{
    if (h == 0) h = 1; // avoid divide-by-zero
    float aspect = (float)w / (float)h;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    // gluPerspective(fovY, aspect, zNear, zFar)
    gluPerspective(60.0, aspect, 0.1, 200.0);

    glMatrixMode(GL_MODELVIEW);
}

// Positions and orients the camera (viewing transformation).
// The camera sits behind and above the ball, looking slightly
// ahead of the ball so the player can anticipate the next platform.
void setupCamera()
{
    glLoadIdentity();

    float eyeX = ball.x;
    float eyeY = ball.y + camera.heightAbove;
    float eyeZ = ball.z + camera.distanceBack; // "behind" = larger Z (ball moves toward -Z)

    float centerX = ball.x;
    float centerY = ball.y + 1.0f;
    float centerZ = ball.z - camera.lookAheadZ; // look ahead, toward -Z

    gluLookAt(eyeX, eyeY, eyeZ,      // eye position
              centerX, centerY, centerZ, // look-at target
              0.0f, 1.0f, 0.0f);     // up vector
}

// =================================================================
// MEMBER 4: Illumination & Materials Engineer
// =================================================================

void setupLighting()
{
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    // Ambient: baseline brightness for the whole scene
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);

    // Diffuse: directional brightness depending on surface normal vs light direction
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);

    // Specular: highlights on shiny surfaces
    glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecular);

    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

    // Use a global ambient term too, and make sure two-sided lighting
    // looks correct for our flat platform polygons.
    GLfloat globalAmbient[] = { 0.15f, 0.15f, 0.18f, 1.0f };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, globalAmbient);

    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    glEnable(GL_NORMALIZE); // keep normals unit length after scaling (squash/stretch)
}

// Apply a "glossy rubber/plastic" material (used for the ball)
void applyGlossyMaterial(float r, float g, float b)
{
    glColor3f(r, g, b); // affects ambient & diffuse via glColorMaterial

    GLfloat specular[] = { 0.9f, 0.9f, 0.9f, 1.0f };
    GLfloat shininess[] = { 64.0f }; // high shininess -> tight, intense highlight
    glMaterialfv(GL_FRONT, GL_SPECULAR, specular);
    glMaterialfv(GL_FRONT, GL_SHININESS, shininess);
}

// Apply a "matte" material (used for platforms / ground)
void applyMatteMaterial(float r, float g, float b)
{
    glColor3f(r, g, b);

    GLfloat specular[] = { 0.05f, 0.05f, 0.05f, 1.0f };
    GLfloat shininess[] = { 4.0f }; // low shininess -> dull, broad/no highlight
    glMaterialfv(GL_FRONT, GL_SPECULAR, specular);
    glMaterialfv(GL_FRONT, GL_SHININESS, shininess);
}

// =================================================================
// MEMBER 5: Rendering & Depth Specialist
// =================================================================

// Configure depth-based fog so platforms fade in smoothly from the
// background instead of suddenly popping into view.
void setupFog()
{
    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_EXP2);   // exponential-squared falloff
    glFogfv(GL_FOG_COLOR, fogColor);
    glFogf(GL_FOG_DENSITY, fogDensity);
    glHint(GL_FOG_HINT, GL_NICEST);
}

// =================================================================
// Scene Initialization
// =================================================================

void initGL()
{
    // ---- Member 5: Visible-Surface Detection (Z-Buffer) ----
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glShadeModel(GL_SMOOTH);
    glClearColor(fogColor[0], fogColor[1], fogColor[2], fogColor[3]);

    // ---- Member 4: Lighting ----
    setupLighting();

    // ---- Member 5: Fog ----
    setupFog();

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
}

// Build the initial ball state, camera config, and the platform sequence.
void initScene()
{
    // ---- Member 3: Ball initial state ----
    ball.x = 0.0f;
    ball.y = 1.0f;
    ball.z = 0.0f;
    ball.radius = 1.0f;
    ball.bounceHeight = 1.5f;
    ball.bouncePeriod = 1.0f;   // 1 second per full bounce cycle
    ball.forwardSpeed = 4.0f;   // units per second
    ball.scaleX = ball.scaleY = ball.scaleZ = 1.0f;

    // ---- Member 1: Camera config ----
    camera.distanceBack = 8.0f;
    camera.heightAbove  = 4.0f;
    camera.lookAheadZ   = 6.0f;

    // ---- Member 2: Generate a sequence of platforms along Z (depth)
    //                and X (left/right), with slight rotations for variety.
    platforms.clear();
    float zStart = -5.0f;
    float zStep  = -8.0f;
    for (int i = 0; i < 25; ++i)
    {
        Platform p;
        p.depth  = 6.0f;
        p.height = 1.0f;
        p.width  = 6.0f;

        // Alternate left/right offset along X
        float xOffsets[] = { 0.0f, 3.0f, -3.0f, 2.0f, -2.0f };
        p.x = xOffsets[i % 5];

        p.y = -1.0f; // top surface roughly at y = -0.5
        p.z = zStart + i * zStep;

        // Slight tilt for visual variety (does not affect ball logic)
        p.rotationY = (i % 3 == 0) ? 8.0f : (i % 3 == 1 ? -8.0f : 0.0f);

        // Alternate colors
        if (i % 2 == 0) { p.r = 0.55f; p.g = 0.35f; p.b = 0.75f; }
        else            { p.r = 0.35f; p.g = 0.65f; p.b = 0.55f; }

        platforms.push_back(p);
    }
}

// =================================================================
// MEMBER 2: Geometric Transformer (World Builder)
// =================================================================

// Draws a single unit cube (1x1x1) centered at the origin.
// Scaling is applied via the modelview matrix by the caller.
void drawUnitCube()
{
    glBegin(GL_QUADS);
        // Front face (+Z)
        glNormal3f(0.0f, 0.0f, 1.0f);
        glVertex3f(-0.5f, -0.5f,  0.5f);
        glVertex3f( 0.5f, -0.5f,  0.5f);
        glVertex3f( 0.5f,  0.5f,  0.5f);
        glVertex3f(-0.5f,  0.5f,  0.5f);

        // Back face (-Z)
        glNormal3f(0.0f, 0.0f, -1.0f);
        glVertex3f( 0.5f, -0.5f, -0.5f);
        glVertex3f(-0.5f, -0.5f, -0.5f);
        glVertex3f(-0.5f,  0.5f, -0.5f);
        glVertex3f( 0.5f,  0.5f, -0.5f);

        // Left face (-X)
        glNormal3f(-1.0f, 0.0f, 0.0f);
        glVertex3f(-0.5f, -0.5f, -0.5f);
        glVertex3f(-0.5f, -0.5f,  0.5f);
        glVertex3f(-0.5f,  0.5f,  0.5f);
        glVertex3f(-0.5f,  0.5f, -0.5f);

        // Right face (+X)
        glNormal3f(1.0f, 0.0f, 0.0f);
        glVertex3f(0.5f, -0.5f,  0.5f);
        glVertex3f(0.5f, -0.5f, -0.5f);
        glVertex3f(0.5f,  0.5f, -0.5f);
        glVertex3f(0.5f,  0.5f,  0.5f);

        // Top face (+Y)
        glNormal3f(0.0f, 1.0f, 0.0f);
        glVertex3f(-0.5f, 0.5f,  0.5f);
        glVertex3f( 0.5f, 0.5f,  0.5f);
        glVertex3f( 0.5f, 0.5f, -0.5f);
        glVertex3f(-0.5f, 0.5f, -0.5f);

        // Bottom face (-Y)
        glNormal3f(0.0f, -1.0f, 0.0f);
        glVertex3f(-0.5f, -0.5f, -0.5f);
        glVertex3f( 0.5f, -0.5f, -0.5f);
        glVertex3f( 0.5f, -0.5f,  0.5f);
        glVertex3f(-0.5f, -0.5f,  0.5f);
    glEnd();
}

// Draws a large flat ground plane using the same unit cube, scaled thin.
void drawGround()
{
    glPushMatrix();
        applyMatteMaterial(0.30f, 0.30f, 0.35f);
        glTranslatef(0.0f, -1.55f, -80.0f);
        glScalef(40.0f, 0.1f, 220.0f);
        drawUnitCube();
    glPopMatrix();
}

// Draws every platform using Translation + Rotation matrices.
// glPushMatrix/glPopMatrix isolate each platform's transform so
// transforming one never affects the rest of the world.
void drawPlatforms()
{
    for (size_t i = 0; i < platforms.size(); ++i)
    {
        const Platform& p = platforms[i];

        glPushMatrix();
            applyMatteMaterial(p.r, p.g, p.b);

            // 3D Translation Matrix: position the platform in the world
            glTranslatef(p.x, p.y, p.z);

            // 3D Rotation Matrix: tilt the platform around the Y axis
            glRotatef(p.rotationY, 0.0f, 1.0f, 0.0f);

            // Scale the unit cube to the platform's dimensions
            glScalef(p.width, p.height, p.depth);

            drawUnitCube();
        glPopMatrix();
    }
}

// =================================================================
// MEMBER 3: Animator
// =================================================================

// Draws a sphere approximation using GLUT, centered at the origin.
// Squash & stretch is applied by the caller via glScalef before this call.
void drawBallGeometry()
{
    glutSolidSphere(ball.radius, 32, 32);
}

// Updates the ball's position and squash/stretch scale based on time.
// - Continuous forward motion: time-based translation along -Z
// - Vertical bounce: |sin| wave for periodic up/down motion
// - Squash & Stretch: when near the ground (impact), compress Y and
//   expand X/Z for a few frames to give a sense of weight and impact.
void updateBall(float dt)
{
    globalTime += dt;

    // Continuous forward translation (time-based)
    ball.z -= ball.forwardSpeed * dt;

    // Periodic vertical motion using a bounce function based on |sin|
    // omega controls how fast the bounce cycles
    float omega = (2.0f * 3.14159265f) / ball.bouncePeriod;
    float bouncePhase = std::fabs(std::sin(globalTime * omega * 0.5f));

    // Ground level for the ball's center when resting
    float groundY = -0.5f + ball.radius;

    ball.y = groundY + bouncePhase * ball.bounceHeight;

    // ---- Squash and Stretch ----
    // Determine how close the ball is to the lowest point of its arc.
    // When bouncePhase is near 0, the ball is at/near impact.
    float impactThreshold = 0.12f;
    if (bouncePhase < impactThreshold)
    {
        // Map [0, impactThreshold] -> squash amount [max squash, none]
        float t = bouncePhase / impactThreshold; // 0 = full impact, 1 = leaving impact
        float squashAmount = (1.0f - t) * 0.35f; // up to 35% deformation

        ball.scaleY = 1.0f - squashAmount;             // squash vertically
        ball.scaleX = 1.0f + squashAmount * 0.5f;      // stretch horizontally
        ball.scaleZ = 1.0f + squashAmount * 0.5f;      // stretch in depth
    }
    else
    {
        ball.scaleX = ball.scaleY = ball.scaleZ = 1.0f; // normal shape
    }
}

// Draws the ball at its current position with the squash/stretch
// non-uniform scale applied via a 3D Scaling Matrix.
void drawBall()
{
    glPushMatrix();
        applyGlossyMaterial(0.95f, 0.25f, 0.25f); // glossy red rubber ball

        glTranslatef(ball.x, ball.y, ball.z);

        // Non-uniform 3D Scaling Matrix for squash & stretch
        glScalef(ball.scaleX, ball.scaleY, ball.scaleZ);

        drawBallGeometry();
    glPopMatrix();
}

// =================================================================
// GLUT Callbacks
// =================================================================

void display()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // ---- Member 1: Viewing transformation (camera) ----
    setupCamera();

    // Re-send light position every frame so it stays correct
    // relative to the (possibly moving) modelview matrix.
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

    // ---- Member 5: Depth test + fog are already enabled in initGL() ----

    // ---- Member 2: World geometry ----
    drawGround();
    drawPlatforms();

    // ---- Member 3: Animated ball ----
    drawBall();

    glutSwapBuffers();
}

void reshape(int w, int h)
{
    windowWidth = w;
    windowHeight = h;

    // ---- Member 1: Viewport mapping ----
    glViewport(0, 0, w, h);

    // ---- Member 1: Perspective projection ----
    setupProjection(w, h);
}

void update(int value)
{
    if (!paused)
    {
        updateBall(TIME_STEP);

        // Loop the level: once the ball passes the last platform area,
        // wrap it back to the start so the demo runs indefinitely.
        if (ball.z < platforms.back().z - 10.0f)
        {
            ball.z = 0.0f;
        }
    }

    glutPostRedisplay();
    glutTimerFunc((unsigned int)(TIME_STEP * 1000.0f), update, 0);
}

void keyboard(unsigned char key, int x, int y)
{
    switch (key)
    {
        case 27: // ESC
            exit(0);
            break;
        case ' ':
            paused = !paused;
            break;
        default:
            break;
    }
}

// =================================================================
// Main
// =================================================================

int main(int argc, char** argv)
{
    glutInit(&argc, argv);
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
