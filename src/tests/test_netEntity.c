/**
 * test_netEntity.c
 * 
 * Unit test file for testing ESVar (Entity State Variable) functionality from netEntity.c.
 * This file tests:
 * - ESVarQueue operations: initialization, adding, popping, and retrieving values
 * - ESDef operations: counting, adding, and handling entity state variables  
 * - Serialization/Deserialization: writing and reading entity states to/from bitstreams
 * - Interpolation: smooth transitions between entity states over time
 * 
 * Run with: ./test_netEntity
 * All tests pass if "All tests passed!" is printed at the end.
 */

#include "../basic/basic.h"
#include "../engine/engine.h"
#include "../engine/entity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

/* ============================================================================
 * STUBS FOR MISSING SYMBOLS (minimal engine dependencies)
 * ============================================================================ */

/* Stub global variables */
EngineParameters engineParameters = {0};
SDL_FRect cameraRect = {0};
cpSpace *worldId = NULL;
server_t server = {0};
client_t client = {0};
VectorEntity vectorEntityList[VECTOR_ENTITY_COUNT] = {0};
NetEntity netEntityList[VECTOR_ENTITY_COUNT] = {0};
SpriteFactory spriteFactoryList[100] = {0};
PlayerData playerDataList[8] = {0};
i2imap_t *mainEntMap = NULL;
entityList_t entList = {0};
entitySpriteList_t entSpriteList = {0};
animatedSpriteList_t animSpriteList = {0};
renderRayList_t renderRayList = {0};
killIDList_t killIDList = {0};
rayWeaponHandle_t rayWeaponHandle = {0};

/* Stub functions for missing symbols */
void world_load(void) {}
void createEdgeBoundary(float x, float y, float width, float height) {}
qbool isSysEventEmpty(void) { return qtrue; }
sysEvent_t *getSysEvent(void) { return NULL; }
void ent_initEntList(void) {}
void ent_handleClientLeave(serv_clrep_t *newClRep) {}
void ent_removeSyncedEntFromClient(int entID, serv_clrep_t *newClRep, int entType) {}
void ent_removeSyncedEntState(int entID, int entType) {}
void ent_settleStateDiff(void) {}
VectorEntity* addSprite(int typeID, SaveDataHandler *saveDataReader, bool isServer, bool isPuppet) { return NULL; }
VectorEntity* getVectorEntity(int spriteID) { return &vectorEntityList[spriteID]; }
netcon_packetstate_e netcon_getPacketState(netcon_t *con, int sequence) { return NETCON_PACKET_SUCCESS; }

/* ============================================================================
 * TEST UTILITIES
 * ============================================================================ */

static int tests_run = 0;
static int tests_passed = 0;

#define TEST_ASSERT(condition, message) do { \
    tests_run++; \
    if (!(condition)) { \
        printf("  FAIL: %s\n", message); \
        printf("    at %s:%d\n", __FILE__, __LINE__); \
    } else { \
        tests_passed++; \
        printf("  PASS: %s\n", message); \
    } \
} while(0)

#define TEST_ASSERT_FLOAT_EQ(a, b, epsilon, message) do { \
    tests_run++; \
    if (fabs((a) - (b)) > (epsilon)) { \
        printf("  FAIL: %s (expected %.6f, got %.6f)\n", message, (double)(b), (double)(a)); \
        printf("    at %s:%d\n", __FILE__, __LINE__); \
    } else { \
        tests_passed++; \
        printf("  PASS: %s\n", message); \
    } \
} while(0)

#define RUN_TEST(test_func) do { \
    printf("\n--- Running %s ---\n", #test_func); \
    test_func(); \
} while(0)

/* ============================================================================
 * MOCK/STUB DATA
 * ============================================================================ */

/**
 * Simple test entity state structure for testing serialization
 */
typedef struct TestEntityState_st {
    float posX;
    float posY;
    float angle;
    int health;
} TestEntityState;

/**
 * Linear interpolation function for float values
 * Uses signed arithmetic to properly handle timestamps before the range
 */
void interpolate_float_linear(void *obj, ESVar *e1, ESVar *e2, uint64_t timestamp) {
    if (e1 == NULL || e2 == NULL || obj == NULL) return;
    
    float v1 = *(float*)e1->buf;
    float v2 = *(float*)e2->buf;
    int64_t t1 = (int64_t)e1->timestamp;
    int64_t t2 = (int64_t)e2->timestamp;
    int64_t ts = (int64_t)timestamp;
    
    if (t2 == t1) {
        *(float*)obj = v2;
        return;
    }
    
    // Use signed arithmetic to avoid underflow
    float alpha = (float)(ts - t1) / (float)(t2 - t1);
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    
    *(float*)obj = v1 + alpha * (v2 - v1);
}

/**
 * Linear interpolation function for int values (rounds to nearest)
 */
void interpolate_int_linear(void *obj, ESVar *e1, ESVar *e2, uint64_t timestamp) {
    if (e1 == NULL || e2 == NULL || obj == NULL) return;
    
    int v1 = *(int*)e1->buf;
    int v2 = *(int*)e2->buf;
    uint64_t t1 = e1->timestamp;
    uint64_t t2 = e2->timestamp;
    
    if (t2 == t1) {
        *(int*)obj = v2;
        return;
    }
    
    float alpha = (float)(timestamp - t1) / (float)(t2 - t1);
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    
    *(int*)obj = (int)(v1 + alpha * (v2 - v1) + 0.5f);
}

/* ============================================================================
 * TEST CASES: ESVarQueue Operations
 * ============================================================================ */

/**
 * Test ESVarQueue initialization
 */
void test_ESVarQueue_init(void) {
    printf("Testing ESVarQueue initialization...\n");
    
    ESVarQueue esQueue;
    int elemSize = sizeof(float);
    int listSize = 4;
    
    initESQueue(&esQueue, elemSize, listSize, NULL);
    
    TEST_ASSERT(esQueue.varList != NULL, "varList should be allocated");
    TEST_ASSERT(esQueue.tsList != NULL, "tsList should be allocated");
    TEST_ASSERT(esQueue.listSize == listSize, "listSize should be set correctly");
    TEST_ASSERT(esQueue.elemSize == elemSize, "elemSize should be set correctly");
    TEST_ASSERT(esQueue.start == 0, "start should be 0");
    TEST_ASSERT(esQueue.end == 0, "end should be 0");
    TEST_ASSERT(esQueue.interpolate == NULL, "interpolate should be NULL");
}

/**
 * Test ESVarQueue empty/full checks
 */
void test_ESVarQueue_empty_full(void) {
    printf("Testing ESVarQueue empty/full state...\n");
    
    ESVarQueue esQueue;
    initESQueue(&esQueue, sizeof(float), 3, NULL);
    
    TEST_ASSERT(isESVQEmpty(&esQueue) == true, "Queue should be empty initially");
    TEST_ASSERT(isESVQFull(&esQueue) == false, "Queue should not be full initially");
    
    // Add items until full
    float val1 = 1.0f, val2 = 2.0f, val3 = 3.0f;
    addESVQueue(&esQueue, &val1, 100);
    TEST_ASSERT(isESVQEmpty(&esQueue) == false, "Queue should not be empty after add");
    
    addESVQueue(&esQueue, &val2, 200);
    addESVQueue(&esQueue, &val3, 300);
    
    TEST_ASSERT(isESVQFull(&esQueue) == true, "Queue should be full after 3 adds");
}

/**
 * Test ESVarQueue add and get operations
 */
void test_ESVarQueue_add_get(void) {
    printf("Testing ESVarQueue add and get...\n");
    
    ESVarQueue esQueue;
    initESQueue(&esQueue, sizeof(float), 4, NULL);
    
    float val1 = 10.5f, val2 = 20.7f, val3 = 30.9f;
    float retrieved;
    
    // Add first value
    addESVQueue(&esQueue, &val1, 100);
    getESVQLast(&esQueue, &retrieved);
    TEST_ASSERT_FLOAT_EQ(retrieved, val1, 0.001f, "Should retrieve first value");
    
    // Add second value
    addESVQueue(&esQueue, &val2, 200);
    getESVQLast(&esQueue, &retrieved);
    TEST_ASSERT_FLOAT_EQ(retrieved, val2, 0.001f, "Should retrieve second value");
    
    // Add third value  
    addESVQueue(&esQueue, &val3, 300);
    getESVQLast(&esQueue, &retrieved);
    TEST_ASSERT_FLOAT_EQ(retrieved, val3, 0.001f, "Should retrieve third value");
}

/**
 * Test ESVarQueue pop operation
 */
void test_ESVarQueue_pop(void) {
    printf("Testing ESVarQueue pop...\n");
    
    ESVarQueue esQueue;
    initESQueue(&esQueue, sizeof(int), 4, NULL);
    
    int val1 = 100, val2 = 200, val3 = 300;
    
    addESVQueue(&esQueue, &val1, 100);
    addESVQueue(&esQueue, &val2, 200);
    addESVQueue(&esQueue, &val3, 300);
    
    int beforeLen = esQueue.end - esQueue.start;
    popESVQueue(&esQueue);
    int afterLen = esQueue.end - esQueue.start;
    
    TEST_ASSERT(afterLen == beforeLen - 1, "Queue length should decrease by 1 after pop");
    
    // Pop until empty
    popESVQueue(&esQueue);
    popESVQueue(&esQueue);
    TEST_ASSERT(isESVQEmpty(&esQueue) == true, "Queue should be empty after popping all");
    
    // Pop on empty should not crash
    popESVQueue(&esQueue);
    TEST_ASSERT(isESVQEmpty(&esQueue) == true, "Pop on empty queue should be safe");
}

/**
 * Test ESVarQueue wraparound behavior
 */
void test_ESVarQueue_wraparound(void) {
    printf("Testing ESVarQueue wraparound...\n");
    
    ESVarQueue esQueue;
    initESQueue(&esQueue, sizeof(int), 3, NULL);
    
    int val1 = 1, val2 = 2, val3 = 3, val4 = 4, val5 = 5;
    int retrieved;
    
    // Fill queue
    addESVQueue(&esQueue, &val1, 100);
    addESVQueue(&esQueue, &val2, 200);
    addESVQueue(&esQueue, &val3, 300);
    
    // Adding to full queue should auto-pop oldest
    addESVQueue(&esQueue, &val4, 400);
    getESVQLast(&esQueue, &retrieved);
    TEST_ASSERT(retrieved == val4, "Should get newest value after overflow");
    
    addESVQueue(&esQueue, &val5, 500);
    getESVQLast(&esQueue, &retrieved);
    TEST_ASSERT(retrieved == val5, "Should get newest value after second overflow");
}

/* ============================================================================
 * TEST CASES: ESDef Operations  
 * ============================================================================ */

/**
 * Test ESDef initialization
 */
void test_ESDef_init(void) {
    printf("Testing ESDef initialization...\n");
    
    ESDef esDef;
    int listSize = 8;
    int snapshotCount = 4;
    
    initESDef(&esDef, listSize, snapshotCount);
    
    TEST_ASSERT(esDef.stateList != NULL, "stateList should be allocated");
    TEST_ASSERT(esDef.listSize == listSize, "listSize should be set");
    TEST_ASSERT(esDef.snapshotCount == snapshotCount, "snapshotCount should be set");
    TEST_ASSERT(esDef.curDef == 0, "curDef should be 0");
}

/**
 * Test ESDef counting state variables
 */
void test_ESDef_count_vars(void) {
    printf("Testing ESDef counting variables...\n");
    
    ESDef esDef;
    initESDef(&esDef, 10, 4);
    
    // Set to counting mode
    setupESDef(&esDef, ESDEF_COUNT, NULL);
    TEST_ASSERT(esDef.curState == ESDEF_COUNT, "State should be ESDEF_COUNT");
    
    // Count variables
    addESVar(&esDef, sizeof(float), NULL);
    addESVar(&esDef, sizeof(float), NULL);
    addESVar(&esDef, sizeof(int), NULL);
    addESVar(&esDef, sizeof(float), NULL);
    
    TEST_ASSERT(esDef.curDef == 4, "Should have counted 4 variables");
}

/**
 * Test ESDef adding state variables
 */
void test_ESDef_add_vars(void) {
    printf("Testing ESDef adding variables...\n");
    
    ESDef esDef;
    initESDef(&esDef, 10, 4);
    
    // Set to add mode
    setupESDef(&esDef, ESDEF_ADD, NULL);
    
    // Add variables with interpolation
    addESVar(&esDef, sizeof(float), interpolate_float_linear);
    addESVar(&esDef, sizeof(float), interpolate_float_linear);
    addESVar(&esDef, sizeof(int), interpolate_int_linear);
    
    // Verify queues were initialized
    TEST_ASSERT(esDef.stateList[0].elemSize == sizeof(float), "First var should be float size");
    TEST_ASSERT(esDef.stateList[1].elemSize == sizeof(float), "Second var should be float size");
    TEST_ASSERT(esDef.stateList[2].elemSize == sizeof(int), "Third var should be int size");
    TEST_ASSERT(esDef.stateList[0].interpolate == interpolate_float_linear, "First var should have interpolate func");
}

/* ============================================================================
 * TEST CASES: Serialization/Deserialization
 * ============================================================================ */

/**
 * Test writing and reading entity state to/from bitstream
 */
void test_serialization_basic(void) {
    printf("Testing basic serialization...\n");
    
    // Create test data
    TestEntityState writeState = {
        .posX = 123.456f,
        .posY = 789.012f,
        .angle = 45.0f,
        .health = 100
    };
    
    TestEntityState readState = {0};
    
    // Create ESDef for writing
    ESDef esDefWrite;
    initESDef(&esDefWrite, 10, 4);
    setupESDef(&esDefWrite, ESDEF_ADD, NULL);
    addESVar(&esDefWrite, sizeof(float), NULL); // posX
    addESVar(&esDefWrite, sizeof(float), NULL); // posY
    addESVar(&esDefWrite, sizeof(float), NULL); // angle
    addESVar(&esDefWrite, sizeof(int), NULL);   // health
    
    // Create bitstream buffer
    byte buffer[256];
    memset(buffer, 0, sizeof(buffer));
    bitstream_t bs;
    stream_init(&bs, buffer, sizeof(buffer));
    
    // Create ESDiff for writing (mark all as should send)
    bool shouldSend[4] = {true, true, true, true};
    ESDiff esDiffWrite = {
        .snapshotIndex = 0,
        .shouldSend = shouldSend
    };
    
    // Write state
    esDefWrite.bs = &bs;
    esDefWrite.esDiff = &esDiffWrite;
    setupESDef(&esDefWrite, ESDEF_BSWRITESTATE, &esDiffWrite);
    esDefWrite.bs = &bs;
    
    handleESVar(&esDefWrite, &writeState.posX);
    handleESVar(&esDefWrite, &writeState.posY);
    handleESVar(&esDefWrite, &writeState.angle);
    handleESVar(&esDefWrite, &writeState.health);
    
    printf("  Written %d bytes, %d bits\n", bs.curbyte, bs.curbit);
    
    // Reset bitstream for reading
    stream_init(&bs, buffer, sizeof(buffer));
    
    // Create ESDef for reading
    ESDef esDefRead;
    initESDef(&esDefRead, 10, 4);
    setupESDef(&esDefRead, ESDEF_ADD, NULL);
    addESVar(&esDefRead, sizeof(float), NULL); // posX
    addESVar(&esDefRead, sizeof(float), NULL); // posY
    addESVar(&esDefRead, sizeof(float), NULL); // angle
    addESVar(&esDefRead, sizeof(int), NULL);   // health
    
    // Read state
    esDefRead.bs = &bs;
    setupESDef(&esDefRead, ESDEF_BSREADSTATE, NULL);
    esDefRead.bs = &bs;
    
    handleESVar(&esDefRead, &readState.posX);
    handleESVar(&esDefRead, &readState.posY);
    handleESVar(&esDefRead, &readState.angle);
    handleESVar(&esDefRead, &readState.health);
    
    // Verify
    TEST_ASSERT_FLOAT_EQ(readState.posX, writeState.posX, 0.001f, "posX should match");
    TEST_ASSERT_FLOAT_EQ(readState.posY, writeState.posY, 0.001f, "posY should match");
    TEST_ASSERT_FLOAT_EQ(readState.angle, writeState.angle, 0.001f, "angle should match");
    TEST_ASSERT(readState.health == writeState.health, "health should match");
}

/**
 * Test selective serialization (only send some fields)
 */
void test_serialization_selective(void) {
    printf("Testing selective serialization...\n");
    
    TestEntityState writeState = {
        .posX = 100.0f,
        .posY = 200.0f,
        .angle = 90.0f,
        .health = 50
    };
    
    TestEntityState readState = {
        .posX = 0.0f,
        .posY = 0.0f,
        .angle = 0.0f,
        .health = 0
    };
    
    byte buffer[256];
    memset(buffer, 0, sizeof(buffer));
    bitstream_t bs;
    stream_init(&bs, buffer, sizeof(buffer));
    
    // Only send posX and health (not posY and angle)
    bool shouldSend[4] = {true, false, false, true};
    ESDiff esDiffWrite = {
        .snapshotIndex = 0,
        .shouldSend = shouldSend
    };
    
    // Write state (ESDef setup)
    ESDef esDefWrite;
    initESDef(&esDefWrite, 10, 4);
    setupESDef(&esDefWrite, ESDEF_ADD, NULL);
    addESVar(&esDefWrite, sizeof(float), NULL);
    addESVar(&esDefWrite, sizeof(float), NULL);
    addESVar(&esDefWrite, sizeof(float), NULL);
    addESVar(&esDefWrite, sizeof(int), NULL);
    
    esDefWrite.bs = &bs;
    esDefWrite.esDiff = &esDiffWrite;
    setupESDef(&esDefWrite, ESDEF_BSWRITESTATE, &esDiffWrite);
    esDefWrite.bs = &bs;
    
    handleESVar(&esDefWrite, &writeState.posX);
    handleESVar(&esDefWrite, &writeState.posY);
    handleESVar(&esDefWrite, &writeState.angle);
    handleESVar(&esDefWrite, &writeState.health);
    
    int bytesWritten = bs.curbyte;
    printf("  Written %d bytes (selective)\n", bytesWritten);
    
    // Read back
    stream_init(&bs, buffer, sizeof(buffer));
    
    ESDef esDefRead;
    initESDef(&esDefRead, 10, 4);
    setupESDef(&esDefRead, ESDEF_ADD, NULL);
    addESVar(&esDefRead, sizeof(float), NULL);
    addESVar(&esDefRead, sizeof(float), NULL);
    addESVar(&esDefRead, sizeof(float), NULL);
    addESVar(&esDefRead, sizeof(int), NULL);
    
    esDefRead.bs = &bs;
    setupESDef(&esDefRead, ESDEF_BSREADSTATE, NULL);
    esDefRead.bs = &bs;
    
    handleESVar(&esDefRead, &readState.posX);
    handleESVar(&esDefRead, &readState.posY);
    handleESVar(&esDefRead, &readState.angle);
    handleESVar(&esDefRead, &readState.health);
    
    // posX and health should be updated, posY and angle should remain 0
    TEST_ASSERT_FLOAT_EQ(readState.posX, writeState.posX, 0.001f, "posX should be updated");
    TEST_ASSERT_FLOAT_EQ(readState.posY, 0.0f, 0.001f, "posY should remain unchanged (not sent)");
    TEST_ASSERT_FLOAT_EQ(readState.angle, 0.0f, 0.001f, "angle should remain unchanged (not sent)");
    TEST_ASSERT(readState.health == writeState.health, "health should be updated");
}

/**
 * Test serialization round-trip with multiple states
 */
void test_serialization_multiple_states(void) {
    printf("Testing multiple state serialization...\n");
    
    // Create several states
    TestEntityState states[3] = {
        {10.0f, 20.0f, 0.0f, 100},
        {30.0f, 40.0f, 45.0f, 90},
        {50.0f, 60.0f, 90.0f, 80}
    };
    
    TestEntityState readStates[3] = {0};
    
    byte buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    bitstream_t bs;
    stream_init(&bs, buffer, sizeof(buffer));
    
    bool shouldSend[4] = {true, true, true, true};
    ESDiff esDiff = {.snapshotIndex = 0, .shouldSend = shouldSend};
    
    // Write all states
    ESDef esDefWrite;
    initESDef(&esDefWrite, 10, 4);
    setupESDef(&esDefWrite, ESDEF_ADD, NULL);
    addESVar(&esDefWrite, sizeof(float), NULL);
    addESVar(&esDefWrite, sizeof(float), NULL);
    addESVar(&esDefWrite, sizeof(float), NULL);
    addESVar(&esDefWrite, sizeof(int), NULL);
    
    for (int i = 0; i < 3; i++) {
        esDefWrite.bs = &bs;
        esDefWrite.esDiff = &esDiff;
        setupESDef(&esDefWrite, ESDEF_BSWRITESTATE, &esDiff);
        esDefWrite.bs = &bs;
        
        handleESVar(&esDefWrite, &states[i].posX);
        handleESVar(&esDefWrite, &states[i].posY);
        handleESVar(&esDefWrite, &states[i].angle);
        handleESVar(&esDefWrite, &states[i].health);
    }
    
    printf("  Written 3 states, %d bytes total\n", bs.curbyte);
    
    // Read all states back
    stream_init(&bs, buffer, sizeof(buffer));
    
    ESDef esDefRead;
    initESDef(&esDefRead, 10, 4);
    setupESDef(&esDefRead, ESDEF_ADD, NULL);
    addESVar(&esDefRead, sizeof(float), NULL);
    addESVar(&esDefRead, sizeof(float), NULL);
    addESVar(&esDefRead, sizeof(float), NULL);
    addESVar(&esDefRead, sizeof(int), NULL);
    
    for (int i = 0; i < 3; i++) {
        esDefRead.bs = &bs;
        setupESDef(&esDefRead, ESDEF_BSREADSTATE, NULL);
        esDefRead.bs = &bs;
        
        handleESVar(&esDefRead, &readStates[i].posX);
        handleESVar(&esDefRead, &readStates[i].posY);
        handleESVar(&esDefRead, &readStates[i].angle);
        handleESVar(&esDefRead, &readStates[i].health);
    }
    
    // Verify all states
    for (int i = 0; i < 3; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "State %d posX should match", i);
        TEST_ASSERT_FLOAT_EQ(readStates[i].posX, states[i].posX, 0.001f, msg);
        snprintf(msg, sizeof(msg), "State %d health should match", i);
        TEST_ASSERT(readStates[i].health == states[i].health, msg);
    }
}

/* ============================================================================
 * TEST CASES: Interpolation
 * ============================================================================ */

/**
 * Test float interpolation function
 */
void test_interpolation_float(void) {
    printf("Testing float interpolation...\n");
    
    float val1 = 0.0f;
    float val2 = 100.0f;
    
    ESVar e1 = {.buf = &val1, .timestamp = 0};
    ESVar e2 = {.buf = &val2, .timestamp = 1000};
    
    float result;
    
    // Test at start
    interpolate_float_linear(&result, &e1, &e2, 0);
    TEST_ASSERT_FLOAT_EQ(result, 0.0f, 0.001f, "Interpolation at t=0 should be 0");
    
    // Test at middle
    interpolate_float_linear(&result, &e1, &e2, 500);
    TEST_ASSERT_FLOAT_EQ(result, 50.0f, 0.001f, "Interpolation at t=500 should be 50");
    
    // Test at end
    interpolate_float_linear(&result, &e1, &e2, 1000);
    TEST_ASSERT_FLOAT_EQ(result, 100.0f, 0.001f, "Interpolation at t=1000 should be 100");
    
    // Test at 25%
    interpolate_float_linear(&result, &e1, &e2, 250);
    TEST_ASSERT_FLOAT_EQ(result, 25.0f, 0.001f, "Interpolation at t=250 should be 25");
    
    // Test at 75%
    interpolate_float_linear(&result, &e1, &e2, 750);
    TEST_ASSERT_FLOAT_EQ(result, 75.0f, 0.001f, "Interpolation at t=750 should be 75");
}

/**
 * Test int interpolation function
 */
void test_interpolation_int(void) {
    printf("Testing int interpolation...\n");
    
    int val1 = 0;
    int val2 = 100;
    
    ESVar e1 = {.buf = &val1, .timestamp = 0};
    ESVar e2 = {.buf = &val2, .timestamp = 1000};
    
    int result;
    
    // Test at start
    interpolate_int_linear(&result, &e1, &e2, 0);
    TEST_ASSERT(result == 0, "Int interpolation at t=0 should be 0");
    
    // Test at middle
    interpolate_int_linear(&result, &e1, &e2, 500);
    TEST_ASSERT(result == 50, "Int interpolation at t=500 should be 50");
    
    // Test at end
    interpolate_int_linear(&result, &e1, &e2, 1000);
    TEST_ASSERT(result == 100, "Int interpolation at t=1000 should be 100");
}

/**
 * Test interpolation clamping (out of range timestamps)
 */
void test_interpolation_clamping(void) {
    printf("Testing interpolation clamping...\n");
    
    float val1 = 10.0f;
    float val2 = 20.0f;
    
    ESVar e1 = {.buf = &val1, .timestamp = 100};
    ESVar e2 = {.buf = &val2, .timestamp = 200};
    
    float result;
    
    // Test before range (should clamp to first value)
    interpolate_float_linear(&result, &e1, &e2, 50);
    TEST_ASSERT_FLOAT_EQ(result, 10.0f, 0.001f, "Before range should clamp to first value");
    
    // Test after range (should clamp to second value)
    interpolate_float_linear(&result, &e1, &e2, 300);
    TEST_ASSERT_FLOAT_EQ(result, 20.0f, 0.001f, "After range should clamp to second value");
}

/**
 * Test interpolation with negative values
 */
void test_interpolation_negative(void) {
    printf("Testing interpolation with negative values...\n");
    
    float val1 = -50.0f;
    float val2 = 50.0f;
    
    ESVar e1 = {.buf = &val1, .timestamp = 0};
    ESVar e2 = {.buf = &val2, .timestamp = 100};
    
    float result;
    
    // Test at middle (should be 0)
    interpolate_float_linear(&result, &e1, &e2, 50);
    TEST_ASSERT_FLOAT_EQ(result, 0.0f, 0.001f, "Interpolation crossing zero should work");
    
    // Test at 25%
    interpolate_float_linear(&result, &e1, &e2, 25);
    TEST_ASSERT_FLOAT_EQ(result, -25.0f, 0.001f, "Negative range interpolation at 25%");
}

/* ============================================================================
 * TEST CASES: ESVarQueue with Timestamps
 * ============================================================================ */

/**
 * Test writing entity state to ESVarQueue
 */
void test_ESVarQueue_write_state(void) {
    printf("Testing ESVarQueue write state...\n");
    
    ESDef esDef;
    initESDef(&esDef, 10, 8);
    
    // Initialize queues in add mode
    setupESDef(&esDef, ESDEF_ADD, NULL);
    addESVar(&esDef, sizeof(float), interpolate_float_linear); // posX
    addESVar(&esDef, sizeof(float), interpolate_float_linear); // posY
    
    // Write state to ESVarQueue
    float posX = 100.0f;
    float posY = 200.0f;
    
    esDef.timestamp = 1000;
    setupESDef(&esDef, ESDEF_WRITETOESV, NULL);
    handleESVar(&esDef, &posX);
    handleESVar(&esDef, &posY);
    
    // Verify values were stored
    float retrievedX, retrievedY;
    getESVQLast(&esDef.stateList[0], &retrievedX);
    getESVQLast(&esDef.stateList[1], &retrievedY);
    
    TEST_ASSERT_FLOAT_EQ(retrievedX, 100.0f, 0.001f, "posX should be stored in queue");
    TEST_ASSERT_FLOAT_EQ(retrievedY, 200.0f, 0.001f, "posY should be stored in queue");
}

/**
 * Test reading entity state from ESVarQueue
 */
void test_ESVarQueue_read_state(void) {
    printf("Testing ESVarQueue read state...\n");
    
    ESDef esDef;
    initESDef(&esDef, 10, 8);
    
    // Initialize queues
    setupESDef(&esDef, ESDEF_ADD, NULL);
    addESVar(&esDef, sizeof(float), NULL);
    addESVar(&esDef, sizeof(float), NULL);
    
    // Write a state
    float writeX = 150.0f;
    float writeY = 250.0f;
    
    esDef.timestamp = 1000;
    setupESDef(&esDef, ESDEF_WRITETOESV, NULL);
    handleESVar(&esDef, &writeX);
    handleESVar(&esDef, &writeY);
    
    // Read state back
    float readX = 0.0f;
    float readY = 0.0f;
    
    setupESDef(&esDef, ESDEF_WRITETOENT, NULL);
    handleESVar(&esDef, &readX);
    handleESVar(&esDef, &readY);
    
    TEST_ASSERT_FLOAT_EQ(readX, 150.0f, 0.001f, "Should read posX from queue");
    TEST_ASSERT_FLOAT_EQ(readY, 250.0f, 0.001f, "Should read posY from queue");
}

/* ============================================================================
 * TEST CASES: Integration Tests
 * ============================================================================ */

/**
 * Full integration test: simulate network entity state sync
 */
void test_integration_entity_sync(void) {
    printf("Testing full entity state sync simulation...\n");
    
    // Server-side: entity with current state
    TestEntityState serverState = {
        .posX = 500.0f,
        .posY = 600.0f,
        .angle = 45.0f,
        .health = 85
    };
    
    // Client-side: entity with old state
    TestEntityState clientState = {
        .posX = 0.0f,
        .posY = 0.0f,
        .angle = 0.0f,
        .health = 0
    };
    
    // Simulate network buffer
    byte networkBuffer[512];
    memset(networkBuffer, 0, sizeof(networkBuffer));
    
    // === SERVER SIDE: Write state ===
    bitstream_t writeBs;
    stream_init(&writeBs, networkBuffer, sizeof(networkBuffer));
    
    ESDef serverEsDef;
    initESDef(&serverEsDef, 10, 4);
    setupESDef(&serverEsDef, ESDEF_ADD, NULL);
    addESVar(&serverEsDef, sizeof(float), NULL);
    addESVar(&serverEsDef, sizeof(float), NULL);
    addESVar(&serverEsDef, sizeof(float), NULL);
    addESVar(&serverEsDef, sizeof(int), NULL);
    
    bool shouldSend[4] = {true, true, true, true};
    ESDiff esDiff = {.snapshotIndex = 0, .shouldSend = shouldSend};
    
    serverEsDef.bs = &writeBs;
    serverEsDef.esDiff = &esDiff;
    setupESDef(&serverEsDef, ESDEF_BSWRITESTATE, &esDiff);
    serverEsDef.bs = &writeBs;
    
    handleESVar(&serverEsDef, &serverState.posX);
    handleESVar(&serverEsDef, &serverState.posY);
    handleESVar(&serverEsDef, &serverState.angle);
    handleESVar(&serverEsDef, &serverState.health);
    
    int packetSize = writeBs.curbyte + (writeBs.curbit > 0 ? 1 : 0);
    printf("  Packet size: %d bytes\n", packetSize);
    
    // === CLIENT SIDE: Read state ===
    bitstream_t readBs;
    stream_init(&readBs, networkBuffer, sizeof(networkBuffer));
    
    ESDef clientEsDef;
    initESDef(&clientEsDef, 10, 4);
    setupESDef(&clientEsDef, ESDEF_ADD, NULL);
    addESVar(&clientEsDef, sizeof(float), NULL);
    addESVar(&clientEsDef, sizeof(float), NULL);
    addESVar(&clientEsDef, sizeof(float), NULL);
    addESVar(&clientEsDef, sizeof(int), NULL);
    
    clientEsDef.bs = &readBs;
    setupESDef(&clientEsDef, ESDEF_BSREADSTATE, NULL);
    clientEsDef.bs = &readBs;
    
    handleESVar(&clientEsDef, &clientState.posX);
    handleESVar(&clientEsDef, &clientState.posY);
    handleESVar(&clientEsDef, &clientState.angle);
    handleESVar(&clientEsDef, &clientState.health);
    
    // Verify client state matches server
    TEST_ASSERT_FLOAT_EQ(clientState.posX, serverState.posX, 0.001f, "Client posX should match server");
    TEST_ASSERT_FLOAT_EQ(clientState.posY, serverState.posY, 0.001f, "Client posY should match server");
    TEST_ASSERT_FLOAT_EQ(clientState.angle, serverState.angle, 0.001f, "Client angle should match server");
    TEST_ASSERT(clientState.health == serverState.health, "Client health should match server");
    
    printf("  Integration test: Server state successfully synced to client!\n");
}

/**
 * Test delta compression scenario (only changed values sent)
 */
void test_integration_delta_compression(void) {
    printf("Testing delta compression scenario...\n");
    
    // Initial state
    TestEntityState state1 = {100.0f, 200.0f, 0.0f, 100};
    // Changed state (only posX changed)
    TestEntityState state2 = {150.0f, 200.0f, 0.0f, 100};
    
    byte fullBuffer[256];
    byte deltaBuffer[256];
    memset(fullBuffer, 0, sizeof(fullBuffer));
    memset(deltaBuffer, 0, sizeof(deltaBuffer));
    
    // Write full state
    bitstream_t fullBs;
    stream_init(&fullBs, fullBuffer, sizeof(fullBuffer));
    
    bool shouldSendAll[4] = {true, true, true, true};
    ESDiff esDiffFull = {.shouldSend = shouldSendAll};
    
    ESDef esDef;
    initESDef(&esDef, 10, 4);
    setupESDef(&esDef, ESDEF_ADD, NULL);
    addESVar(&esDef, sizeof(float), NULL);
    addESVar(&esDef, sizeof(float), NULL);
    addESVar(&esDef, sizeof(float), NULL);
    addESVar(&esDef, sizeof(int), NULL);
    
    esDef.bs = &fullBs;
    esDef.esDiff = &esDiffFull;
    setupESDef(&esDef, ESDEF_BSWRITESTATE, &esDiffFull);
    esDef.bs = &fullBs;
    handleESVar(&esDef, &state1.posX);
    handleESVar(&esDef, &state1.posY);
    handleESVar(&esDef, &state1.angle);
    handleESVar(&esDef, &state1.health);
    
    int fullSize = fullBs.curbyte;
    
    // Write delta state (only posX changed)
    bitstream_t deltaBs;
    stream_init(&deltaBs, deltaBuffer, sizeof(deltaBuffer));
    
    bool shouldSendDelta[4] = {true, false, false, false}; // Only posX
    ESDiff esDiffDelta = {.shouldSend = shouldSendDelta};
    
    esDef.bs = &deltaBs;
    esDef.esDiff = &esDiffDelta;
    setupESDef(&esDef, ESDEF_BSWRITESTATE, &esDiffDelta);
    esDef.bs = &deltaBs;
    handleESVar(&esDef, &state2.posX);
    handleESVar(&esDef, &state2.posY);
    handleESVar(&esDef, &state2.angle);
    handleESVar(&esDef, &state2.health);
    
    int deltaSize = deltaBs.curbyte;
    
    printf("  Full state size: %d bytes\n", fullSize);
    printf("  Delta state size: %d bytes\n", deltaSize);
    
    TEST_ASSERT(deltaSize < fullSize, "Delta should be smaller than full state");
}

/* ============================================================================
 * MAIN TEST RUNNER
 * ============================================================================ */

int main(int argc, char *argv[]) {
    printf("========================================\n");
    printf("ESVar (Entity State Variable) Test Suite\n");
    printf("========================================\n");
    
    // Initialize memory zones (required by the engine)
    createThreeZones(1024 * 1024, 512 * 1024, 256 * 1024);
    
    // Run ESVarQueue tests
    RUN_TEST(test_ESVarQueue_init);
    RUN_TEST(test_ESVarQueue_empty_full);
    RUN_TEST(test_ESVarQueue_add_get);
    RUN_TEST(test_ESVarQueue_pop);
    RUN_TEST(test_ESVarQueue_wraparound);
    
    // Run ESDef tests
    RUN_TEST(test_ESDef_init);
    RUN_TEST(test_ESDef_count_vars);
    RUN_TEST(test_ESDef_add_vars);
    
    // Run Serialization tests
    RUN_TEST(test_serialization_basic);
    RUN_TEST(test_serialization_selective);
    RUN_TEST(test_serialization_multiple_states);
    
    // Run Interpolation tests
    RUN_TEST(test_interpolation_float);
    RUN_TEST(test_interpolation_int);
    RUN_TEST(test_interpolation_clamping);
    RUN_TEST(test_interpolation_negative);
    
    // Run ESVarQueue state tests
    RUN_TEST(test_ESVarQueue_write_state);
    RUN_TEST(test_ESVarQueue_read_state);
    
    // Run Integration tests
    RUN_TEST(test_integration_entity_sync);
    RUN_TEST(test_integration_delta_compression);
    
    // Summary
    printf("\n========================================\n");
    printf("Test Results: %d/%d passed\n", tests_passed, tests_run);
    printf("========================================\n");
    
    if (tests_passed == tests_run) {
        printf("\n*** All tests passed! ***\n\n");
        return 0;
    } else {
        printf("\n*** Some tests failed! ***\n\n");
        return 1;
    }
}
