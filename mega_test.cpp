#include "uthreads.h"
#include <iostream>
#include <vector>
#include <cstdlib>

// מאקרו לבדיקות - אם התנאי שקר, מדפיס שגיאה ועוצר
#define ASSERT_TRUE(cond, msg) \
    if (!(cond)) { \
        std::cerr << "\n[❌ FAILED] " << msg << std::endl; \
        exit(1); \
    } else { \
        std::cout << "[✅ PASSED] " << msg << std::endl; \
    }

// --- חוטים לדוגמה ---

void generic_thread() {
    while(true) {
        uthread_sleep(0); // רק מוותר על התור
    }
}

void sleep_and_block_thread() {
    std::cout << "   -> Thread " << uthread_get_tid() << " is going to sleep for 3 quantums." << std::endl;
    uthread_sleep(3);

    // אם הגענו לכאן, סימן שגם עברו 3 קוונטומים וגם מישהו עשה לנו Resume (במידה ונחסמנו)
    std::cout << "   -> Thread " << uthread_get_tid() << " woke up!" << std::endl;
    uthread_terminate(uthread_get_tid());
}

void suicide_thread() {
    std::cout << "   -> Thread " << uthread_get_tid() << " terminating itself." << std::endl;
    uthread_terminate(uthread_get_tid());
}

// --- פונקציית ה-Main והטסט עצמו ---

int main() {
    std::cout << "========== STARTING MEGA TEST ==========\n" << std::endl;

    // שלב 0: אתחול
    std::cout << "--- Phase 0: Initialization ---" << std::endl;
    ASSERT_TRUE(uthread_init(0) == -1, "Init with 0 quantum should fail");
    ASSERT_TRUE(uthread_init(-5) == -1, "Init with negative quantum should fail");
    ASSERT_TRUE(uthread_init(999999) == 0, "Init with valid quantum should succeed");

    // שלב 1: ניהול מזהים (TIDs)
    std::cout << "\n--- Phase 1: TID Management ---" << std::endl;
    int t1 = uthread_spawn(generic_thread);
    int t2 = uthread_spawn(generic_thread);
    int t3 = uthread_spawn(generic_thread);
    ASSERT_TRUE(t1 == 1 && t2 == 2 && t3 == 3, "Spawning should allocate smallest available IDs (1, 2, 3)");

    ASSERT_TRUE(uthread_terminate(2) == 0, "Terminating thread 2 should succeed");
    int t4 = uthread_spawn(generic_thread);
    ASSERT_TRUE(t4 == 2, "Spawning new thread should reuse the smallest available ID (2)");

    // שלב 2: קלטים לא חוקיים ושגיאות
    std::cout << "\n--- Phase 2: Invalid Inputs & Error Handling ---" << std::endl;
    ASSERT_TRUE(uthread_terminate(-1) == -1, "Terminating negative TID should fail");
    ASSERT_TRUE(uthread_terminate(1000) == -1, "Terminating out of bounds TID should fail");
    ASSERT_TRUE(uthread_terminate(50) == -1, "Terminating non-existent thread should fail");
    ASSERT_TRUE(uthread_block(0) == -1, "Blocking the main thread (0) should fail");
    ASSERT_TRUE(uthread_block(-5) == -1, "Blocking invalid TID should fail");
    ASSERT_TRUE(uthread_resume(99) == -1, "Resuming non-existent thread should fail");
    ASSERT_TRUE(uthread_sleep(-1) == -1, "Sleeping for negative quantums should fail");

    // שלב 3: חסימה (Block) ושחרור (Resume)
    std::cout << "\n--- Phase 3: Block and Resume ---" << std::endl;
    ASSERT_TRUE(uthread_block(3) == 0, "Blocking thread 3 should succeed");
    ASSERT_TRUE(uthread_block(3) == 0, "Double-blocking thread 3 should not crash (should return 0)");

    // ניתן ל-T3 הזדמנות לרוץ. אם הוא באמת חסום, הוא לא יעשה כלום והתור יחזור אלינו מהר
    for(int i=0; i<3; i++) uthread_sleep(0);

    ASSERT_TRUE(uthread_resume(3) == 0, "Resuming thread 3 should succeed");
    ASSERT_TRUE(uthread_resume(3) == 0, "Double-resuming thread 3 should return 0 (no effect)");

    // שלב 4: השילוב המאתגר - שינה וחסימה ביחד
    std::cout << "\n--- Phase 4: Block + Sleep Combo ---" << std::endl;
    int t5 = uthread_spawn(sleep_and_block_thread); // מקבל ID 4

    // מעבירים תור כדי ש-T5 ירוץ ויירדם ל-3 קוונטומים
    uthread_sleep(0);

    // חוסמים אותו בזמן שהוא ישן
    ASSERT_TRUE(uthread_block(t5) == 0, "Blocking a sleeping thread should succeed");

    // שורפים 5 קוונטומים (יותר מה-3 שהוא צריך לישון)
    for(int i=0; i<5; i++) {
        uthread_sleep(0);
    }

    // למרות שזמן השינה עבר, הוא עדיין חסום ולכן לא היה אמור לסיים!
    // נשחרר אותו עכשיו
    ASSERT_TRUE(uthread_resume(t5) == 0, "Resuming the thread after sleep expired");

    // נותנים לו לרוץ כדי להדפיס הודעה ולחסל את עצמו
    uthread_sleep(0);

    // שלב 5: הגבלת כמות חוטים (MAX_THREAD_NUM)
    std::cout << "\n--- Phase 5: Thread Limit Exhaustion ---" << std::endl;
    std::vector<int> spawned;
    int spawn_count = 0;
    while(true) {
        int tid = uthread_spawn(generic_thread);
        if (tid == -1) break; // הגענו למקסימום
        spawned.push_back(tid);
        spawn_count++;
        if (spawn_count > 150) {
            std::cerr << "\n[❌ FAILED] Exceeded expected MAX_THREAD_NUM without returning -1!" << std::endl;
            exit(1);
        }
    }
    std::cout << "   -> Successfully hit the MAX_THREAD_NUM limit. Spawned " << spawn_count << " new threads." << std::endl;
    ASSERT_TRUE(spawn_count > 50, "Limit test completed and verified");

    // ניקוי המערכת - חיסול כל החוטים שיצרנו (לבדוק שזה לא קורס תחת עומס)
    for(int tid : spawned) {
        uthread_terminate(tid);
    }
    uthread_terminate(1);
    uthread_terminate(2);
    uthread_terminate(3);

    // מוודאים שאפשר ליצור חוט חדש אחרי שניקינו
    int t6 = uthread_spawn(suicide_thread);
    ASSERT_TRUE(t6 == 1, "After massive cleanup, next TID should be 1 again");
    uthread_sleep(0); // נותנים לו להרוג את עצמו

    // שלב 6: ספירת קוונטומים
    std::cout << "\n--- Phase 6: Quantum Counters ---" << std::endl;
    int total_q = uthread_get_total_quantums();
    int main_q = uthread_get_quantums(0);

    std::cout << "   -> Total Quantums: " << total_q << std::endl;
    std::cout << "   -> Main Thread Quantums: " << main_q << std::endl;

    ASSERT_TRUE(total_q > 15, "Total quantums counter is incrementing correctly");
    ASSERT_TRUE(main_q > 5, "Main thread quantum counter is incrementing correctly");

    std::cout << "\n========== ALL TESTS PASSED SUCCESSFULLY! ==========" << std::endl;

    // סיום תקין של המערכת
    uthread_terminate(0);
    return 0; // לא אמור להגיע לכאן
}