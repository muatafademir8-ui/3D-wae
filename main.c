#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspctrl.h>
#include <pspdebug.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

PSP_MODULE_INFO("PanzerStormPSP", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define BUF_WIDTH  512
#define SCR_WIDTH  480
#define SCR_HEIGHT 272
#define PI 3.14159265f

static unsigned int __attribute__((aligned(16))) list[262144];

typedef struct {
    unsigned int color;
    float x, y, z;
} Vertex;

// 3D Kup Modeli (36 Vertex)
Vertex __attribute__((aligned(16))) cubeVertices[36] = {
    // On
    {0xFF3A5A40, -0.5f, -0.5f,  0.5f}, {0xFF3A5A40,  0.5f, -0.5f,  0.5f}, {0xFF588157,  0.5f,  0.5f,  0.5f},
    {0xFF3A5A40, -0.5f, -0.5f,  0.5f}, {0xFF588157,  0.5f,  0.5f,  0.5f}, {0xFF588157, -0.5f,  0.5f,  0.5f},
    // Arka
    {0xFF283618,  0.5f, -0.5f, -0.5f}, {0xFF283618, -0.5f, -0.5f, -0.5f}, {0xFF344E41, -0.5f,  0.5f, -0.5f},
    {0xFF283618,  0.5f, -0.5f, -0.5f}, {0xFF344E41, -0.5f,  0.5f, -0.5f}, {0xFF344E41,  0.5f,  0.5f, -0.5f},
    // Ust
    {0xFFA3B18A, -0.5f,  0.5f,  0.5f}, {0xFFA3B18A,  0.5f,  0.5f,  0.5f}, {0xFFA3B18A,  0.5f,  0.5f, -0.5f},
    {0xFFA3B18A, -0.5f,  0.5f,  0.5f}, {0xFFA3B18A,  0.5f,  0.5f, -0.5f}, {0xFFA3B18A, -0.5f,  0.5f, -0.5f},
    // Alt
    {0xFF1B261B, -0.5f, -0.5f, -0.5f}, {0xFF1B261B,  0.5f, -0.5f, -0.5f}, {0xFF1B261B,  0.5f, -0.5f,  0.5f},
    {0xFF1B261B, -0.5f, -0.5f, -0.5f}, {0xFF1B261B,  0.5f, -0.5f,  0.5f}, {0xFF1B261B, -0.5f, -0.5f,  0.5f},
    // Sol
    {0xFF344E41, -0.5f, -0.5f, -0.5f}, {0xFF344E41, -0.5f, -0.5f,  0.5f}, {0xFF588157, -0.5f,  0.5f,  0.5f},
    {0xFF344E41, -0.5f, -0.5f, -0.5f}, {0xFF588157, -0.5f,  0.5f,  0.5f}, {0xFF588157, -0.5f,  0.5f, -0.5f},
    // Sag
    {0xFF344E41,  0.5f, -0.5f,  0.5f}, {0xFF344E41,  0.5f, -0.5f, -0.5f}, {0xFF588157,  0.5f,  0.5f, -0.5f},
    {0xFF344E41,  0.5f, -0.5f,  0.5f}, {0xFF588157,  0.5f,  0.5f, -0.5f}, {0xFF588157,  0.5f,  0.5f,  0.5f}
};

// Kirmizi Dusman Kupu
Vertex __attribute__((aligned(16))) enemyCubeVertices[36];

// Zemin Grid Vertexleri
#define GRID_LINES 44
Vertex __attribute__((aligned(16))) gridVertices[GRID_LINES * 2];

typedef struct {
    float x, z;
    float angle;
    float turretAngle;
    int hp, maxHp;
    int scrap;
    int score;
    int wave;
    int reloadTimer;
    int upgArmor;
    int upgCannon;
    int upgReload;
    int upgEngine;
} PlayerTank;

typedef struct {
    int active;
    float x, z;
    float angle;
    int hp;
    int reloadTimer;
    int isHeavy;
} EnemyTank;

typedef struct {
    int active;
    int fromPlayer;
    float x, y, z;
    float vx, vz;
    int dmg;
    int life;
} Bullet;

#define MAX_ENEMIES 12
#define MAX_BULLETS 40
#define MAX_OBSTACLES 24

PlayerTank player;
EnemyTank enemies[MAX_ENEMIES];
Bullet bullets[MAX_BULLETS];
float obsX[MAX_OBSTACLES], obsZ[MAX_OBSTACLES], obsScale[MAX_OBSTACLES];

int garageOpen = 0;
int garageCursor = 0;
unsigned int oldButtons = 0;

int exit_callback(int arg1, int arg2, void *common) {
    sceKernelExitGame();
    return 0;
}

int CallbackThread(SceSize args, void *argp) {
    int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

void SetupCallbacks(void) {
    int thid = sceKernelCreateThread("update_thread", CallbackThread, 0x11, 0xFA0, 0, 0);
    if (thid >= 0) sceKernelStartThread(thid, 0, 0);
}

void InitGeometry(void) {
    for (int i = 0; i < 36; i++) {
        enemyCubeVertices[i] = cubeVertices[i];
        // Yesil tonlarini kirmiziya cevir (ABGR formati)
        unsigned int c = cubeVertices[i].color;
        unsigned int g = (c >> 8) & 0xFF;
        enemyCubeVertices[i].color = 0xFF151500 | g;
    }

    int idx = 0;
    for (int i = -10; i <= 10; i++) {
        float pos = i * 8.0f;
        gridVertices[idx++] = (Vertex){0xFF334433, pos, 0.0f, -80.0f};
        gridVertices[idx++] = (Vertex){0xFF334433, pos, 0.0f,  80.0f};
        gridVertices[idx++] = (Vertex){0xFF334433, -80.0f, 0.0f, pos};
        gridVertices[idx++] = (Vertex){0xFF334433,  80.0f, 0.0f, pos};
    }

    for (int i = 0; i < MAX_OBSTACLES; i++) {
        float a = (float)i * 0.65f;
        float d = 18.0f + (i % 7) * 9.0f;
        obsX[i] = cosf(a) * d;
        obsZ[i] = sinf(a) * d;
        obsScale[i] = 2.2f + (i % 3) * 1.1f;
    }
}

void SpawnWave(int wave) {
    int count = 2 + wave * 2;
    if (count > MAX_ENEMIES) count = MAX_ENEMIES;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (i < count) {
            float a = (2.0f * PI * i) / count;
            float dist = 35.0f + (i % 3) * 12.0f;
            enemies[i].active = 1;
            enemies[i].x = player.x + sinf(a) * dist;
            enemies[i].z = player.z + cosf(a) * dist;
            enemies[i].angle = a + PI;
            enemies[i].isHeavy = (wave >= 2 && i % 3 == 0) ? 1 : 0;
            enemies[i].hp = enemies[i].isHeavy ? (90 + wave * 20) : (45 + wave * 12);
            enemies[i].reloadTimer = 60 + i * 15;
        } else {
            enemies[i].active = 0;
        }
    }
}

void InitGame(void) {
    memset(&player, 0, sizeof(PlayerTank));
    player.hp = 100;
    player.maxHp = 100;
    player.scrap = 150;
    player.wave = 1;
    player.upgArmor = 1;
    player.upgCannon = 1;
    player.upgReload = 1;
    player.upgEngine = 1;

    memset(bullets, 0, sizeof(bullets));
    SpawnWave(1);
}

void FireBullet(int fromPlayer, float x, float z, float angle, int dmg, float speed) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) {
            bullets[i].active = 1;
            bullets[i].fromPlayer = fromPlayer;
            bullets[i].x = x + sinf(angle) * 2.2f;
            bullets[i].y = 1.1f;
            bullets[i].z = z + cosf(angle) * 2.2f;
            bullets[i].vx = sinf(angle) * speed;
            bullets[i].vz = cosf(angle) * speed;
            bullets[i].dmg = dmg;
            bullets[i].life = 90;
            break;
        }
    }
}

void DrawBox(float x, float y, float z, float sx, float sy, float sz, float rotY, Vertex* verts) {
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    ScePspFVector3 pos = {x, y, z};
    ScePspFVector3 scl = {sx, sy, sz};
    sceGumTranslate(&pos);
    sceGumRotateY(rotY);
    sceGumScale(&scl);
    sceGumDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 36, 0, verts);
}

void DrawTank3D(float x, float z, float bodyAngle, float turretAngle, int isPlayer, int isHeavy) {
    Vertex* v = isPlayer ? cubeVertices : enemyCubeVertices;
    float s = isHeavy ? 1.35f : 1.0f;
    // Govde
    DrawBox(x, 0.55f * s, z, 2.2f * s, 0.9f * s, 3.0f * s, bodyAngle, v);
    // Kule
    DrawBox(x, 1.25f * s, z, 1.4f * s, 0.7f * s, 1.5f * s, turretAngle, v);
    // Top Namlusu
    float bx = x + sinf(turretAngle) * (1.7f * s);
    float bz = z + cosf(turretAngle) * (1.7f * s);
    DrawBox(bx, 1.25f * s, bz, 0.25f * s, 0.25f * s, 2.2f * s, turretAngle, v);
}

void UpdateGame(SceCtrlData* pad) {
    unsigned int pressed = pad->Buttons & ~oldButtons;
    oldButtons = pad->Buttons;

    if (pressed & PSP_CTRL_SELECT) {
        garageOpen = !garageOpen;
        if (garageOpen && player.hp < player.maxHp) player.hp += 10;
        if (player.hp > player.maxHp) player.hp = player.maxHp;
    }

    if (garageOpen) {
        if (pressed & PSP_CTRL_UP) garageCursor = (garageCursor + 3) % 4;
        if (pressed & PSP_CTRL_DOWN) garageCursor = (garageCursor + 1) % 4;
        if (pressed & PSP_CTRL_CROSS) {
            int costs[4] = {
                80 * player.upgArmor,
                100 * player.upgCannon,
                90 * player.upgReload,
                75 * player.upgEngine
            };
            int* levels[4] = {&player.upgArmor, &player.upgCannon, &player.upgReload, &player.upgEngine};
            if (*levels[garageCursor] < 5 && player.scrap >= costs[garageCursor]) {
                player.scrap -= costs[garageCursor];
                (*levels[garageCursor])++;
                if (garageCursor == 0) {
                    player.maxHp = 100 + (player.upgArmor - 1) * 35;
                    player.hp = player.maxHp;
                }
            }
        }
        return;
    }

    // Analog Stick ve D-Pad Surus
    float ax = ((float)pad->Lx - 128.0f) / 128.0f;
    float ay = ((float)pad->Ly - 128.0f) / 128.0f;
    if (fabsf(ax) < 0.2f) ax = 0.0f;
    if (fabsf(ay) < 0.2f) ay = 0.0f;

    if (pad->Buttons & PSP_CTRL_UP) ay = -1.0f;
    if (pad->Buttons & PSP_CTRL_DOWN) ay = 1.0f;
    if (pad->Buttons & PSP_CTRL_LEFT) ax = -1.0f;
    if (pad->Buttons & PSP_CTRL_RIGHT) ax = 1.0f;

    float moveSpd = 0.22f + (player.upgEngine - 1) * 0.045f;
    float rotSpd = 0.042f + (player.upgEngine - 1) * 0.007f;

    player.angle -= ax * rotSpd;
    player.turretAngle -= ax * rotSpd;
    player.x += sinf(player.angle) * (-ay) * moveSpd;
    player.z += cosf(player.angle) * (-ay) * moveSpd;

    // L / R ile Kule Dondurme
    if (pad->Buttons & PSP_CTRL_LTRIGGER) player.turretAngle += 0.045f;
    if (pad->Buttons & PSP_CTRL_RTRIGGER) player.turretAngle -= 0.045f;

    // Atesleme (X: Ana Top, O: Makineli Tufek)
    if (player.reloadTimer > 0) player.reloadTimer--;
    if ((pad->Buttons & PSP_CTRL_CROSS) && player.reloadTimer <= 0) {
        int dmg = 40 + (player.upgCannon - 1) * 18;
        FireBullet(1, player.x, player.z, player.turretAngle, dmg, 1.35f);
        player.reloadTimer = 55 - (player.upgReload - 1) * 8;
        if (player.reloadTimer < 18) player.reloadTimer = 18;
    }
    if ((pressed & PSP_CTRL_CIRCLE)) {
        int mgDmg = 10 + (player.upgCannon - 1) * 4;
        FireBullet(1, player.x, player.z, player.turretAngle, mgDmg, 1.65f);
    }

    // Dusman AI
    int activeEnemies = 0;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) continue;
        activeEnemies++;
        float dx = player.x - enemies[i].x;
        float dz = player.z - enemies[i].z;
        float dist = sqrtf(dx * dx + dz * dz);
        enemies[i].angle = atan2f(dx, dz);

        if (dist > 14.0f) {
            float spd = enemies[i].isHeavy ? 0.09f : 0.14f;
            enemies[i].x += sinf(enemies[i].angle) * spd;
            enemies[i].z += cosf(enemies[i].angle) * spd;
        }

        enemies[i].reloadTimer--;
        if (enemies[i].reloadTimer <= 0 && dist < 55.0f) {
            FireBullet(0, enemies[i].x, enemies[i].z, enemies[i].angle, enemies[i].isHeavy ? 18 : 10, 0.85f);
            enemies[i].reloadTimer = enemies[i].isHeavy ? 120 : 90;
        }
    }

    if (activeEnemies == 0) {
        player.wave++;
        SpawnWave(player.wave);
    }

    // Mermiler
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) continue;
        bullets[i].x += bullets[i].vx;
        bullets[i].z += bullets[i].vz;
        bullets[i].life--;
        if (bullets[i].life <= 0) {
            bullets[i].active = 0;
            continue;
        }

        if (bullets[i].fromPlayer) {
            for (int j = 0; j < MAX_ENEMIES; j++) {
                if (!enemies[j].active) continue;
                float dx = bullets[i].x - enemies[j].x;
                float dz = bullets[i].z - enemies[j].z;
                if (dx * dx + dz * dz < 4.5f) {
                    enemies[j].hp -= bullets[i].dmg;
                    bullets[i].active = 0;
                    if (enemies[j].hp <= 0) {
                        enemies[j].active = 0;
                        player.scrap += enemies[j].isHeavy ? 65 : 35;
                        player.score += enemies[j].isHeavy ? 250 : 120;
                    }
                    break;
                }
            }
        } else {
            float dx = bullets[i].x - player.x;
            float dz = bullets[i].z - player.z;
            if (dx * dx + dz * dz < 4.0f) {
                player.hp -= bullets[i].dmg;
                bullets[i].active = 0;
                if (player.hp <= 0) {
                    player.hp = player.maxHp;
                    player.x = 0.0f;
                    player.z = 0.0f;
                    player.scrap = (player.scrap * 7) / 10;
                }
            }
        }
    }
}

int main(void) {
    SetupCallbacks();
    pspDebugScreenInit();

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    InitGeometry();
    InitGame();

    sceGuInit();
    sceGuStart(GU_DIRECT, list);
    sceGuDrawBuffer(GU_PSM_8888, (void*)0, BUF_WIDTH);
    sceGuDispBuffer(SCR_WIDTH, SCR_HEIGHT, (void*)0x88000, BUF_WIDTH);
    sceGuDepthBuffer((void*)0x110000, BUF_WIDTH);
    sceGuOffset(2048 - (SCR_WIDTH / 2), 2048 - (SCR_HEIGHT / 2));
    sceGuViewport(2048, 2048, SCR_WIDTH, SCR_HEIGHT);
    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, SCR_WIDTH, SCR_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuFrontFace(GU_CCW);
    sceGuShadeModel(GU_SMOOTH);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);

    SceCtrlData pad;

    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);
        UpdateGame(&pad);

        sceGuStart(GU_DIRECT, list);
        sceGuClearColor(0xFF281D14); // PSP Atmosferik Gokyuzu
        sceGuClearDepth(0);
        sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

        sceGumMatrixMode(GU_PROJECTION);
        sceGumLoadIdentity();
        sceGumPerspective(60.0f, 16.0f / 9.0f, 0.5f, 200.0f);

        sceGumMatrixMode(GU_VIEW);
        sceGumLoadIdentity();
        ScePspFVector3 eye = {
            player.x - sinf(player.turretAngle) * 11.5f,
            5.2f,
            player.z - cosf(player.turretAngle) * 11.5f
        };
        ScePspFVector3 center = {
            player.x + sinf(player.turretAngle) * 12.0f,
            1.8f,
            player.z + cosf(player.turretAngle) * 12.0f
        };
        ScePspFVector3 up = {0.0f, 1.0f, 0.0f};
        sceGumLookAt(&eye, &center, &up);

        // Zemin Grid Cizimi
        sceGumMatrixMode(GU_MODEL);
        sceGumLoadIdentity();
        sceGumDrawArray(GU_LINES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, GRID_LINES * 2, 0, gridVertices);

        // Engeller
        for (int i = 0; i < MAX_OBSTACLES; i++) {
            DrawBox(obsX[i], obsScale[i] * 0.5f, obsZ[i], obsScale[i], obsScale[i], obsScale[i], 0.0f, cubeVertices);
        }

        // Oyuncu Tanki
        DrawTank3D(player.x, player.z, player.angle, player.turretAngle, 1, 0);

        // Dusman Tanklari
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (enemies[i].active) {
                DrawTank3D(enemies[i].x, enemies[i].z, enemies[i].angle, enemies[i].angle, 0, enemies[i].isHeavy);
            }
        }

        // Mermiler
        for (int i = 0; i < MAX_BULLETS; i++) {
            if (bullets[i].active) {
                DrawBox(bullets[i].x, bullets[i].y, bullets[i].z, 0.35f, 0.35f, 0.35f, 0.0f, enemyCubeVertices);
            }
        }

        sceGuFinish();
        sceGuSync(0, 0);

        // PSP Debug HUD / Garaj Arayuzu
        pspDebugScreenSetXY(1, 1);
        pspDebugScreenPrintf("PANZER STORM PSP | DALGA:%d | ZIRH:%d/%d | HURDA:%d | SKOR:%d",
            player.wave, player.hp, player.maxHp, player.scrap, player.score);

        if (garageOpen) {
            pspDebugScreenSetXY(4, 6);
            pspDebugScreenPrintf("=== TANK GARAJI (SELECT: KAPAT | X: YUKSELT) ===");
            const char* names[4] = {"Kompozit Zirh", "120mm Top Namlusu", "Otomatik Doldurucu", "V12 Turbo Motor"};
            int levels[4] = {player.upgArmor, player.upgCannon, player.upgReload, player.upgEngine};
            int costs[4] = {80 * player.upgArmor, 100 * player.upgCannon, 90 * player.upgReload, 75 * player.upgEngine};
            for (int i = 0; i < 4; i++) {
                pspDebugScreenSetXY(6, 9 + i * 2);
                pspDebugScreenPrintf("%s %-20s [Sv.%d/5] - Maliyet: %d Hurda",
                    (garageCursor == i) ? "->" : "  ", names[i], levels[i], costs[i]);
            }
        } else {
            pspDebugScreenSetXY(1, 32);
            pspDebugScreenPrintf("[Analog/Yon] Sur | [L/R] Kule | [X] Top | [O] Makineli | [SELECT] Garaj");
        }

        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }

    return 0;
}
