#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SCREEN_WIDTH 900
#define SCREEN_HEIGHT 700
#define MAX_INPUT 500

// leaderboard/history
#define LEADERBOARD_FILE "leaderboard.txt"
#define HISTORY_FILE "history.log"
#define LEADER_COUNT 5

typedef struct
{
    char name[32];
    int wpm;
    float accuracy;
} LeaderEntry;

LeaderEntry leaderboard[LEADER_COUNT];
int leaderboardLoaded = 0;

// array for display ranking (indices into leaderboard[])
int leaderboardOrder[LEADER_COUNT];

// Sample texts
char *codeTexts[] = {
    "int main() { return 0; }",
    "for(int i=0; i<10; i++) { }",
    "char* str = \"Hello\";",
    "if(x > 0) { sum = x + y; }",
    "#include <stdio.h>"};

char *standardTexts[] = {
    "The quick brown fox jumps over the lazy dog.",
    "Practice makes perfect in everything you do.",
    "Time flies when you are having fun.",
    "Knowledge is power and typing is efficiency.",
    "Every journey begins with a single step."};

char *sprintTexts[] = {
    "the and for are you can have",
    "one two three four five six",
    "cat dog run jump fly sit",
    "yes now try get see use",
    "all new big old red hot"};

// Program states
int currentState = 0; // 0=MODE_SELECT, 1=TESTING, 2=RESULTS, 3=LEADERBOARD_VIEW
int selectedMode = 0; // 1=Code, 2=Standard, 3=Sprint
char *currentText = NULL;
char userInput[MAX_INPUT] = "";
int inputlen = 0;
time_t startTime = 0;
int testActive = 0;
int wpm = 0;
float accuracy = 0.0f;
int totalWords = 0;
int timeTaken = 0;

// Functions
int countWords(char *text)
{
    int count = 0, inWord = 0;
    for (int i = 0; text[i] != '\0'; i++)
    {
        if (text[i] == ' ')
        {
            inWord = 0;
        }
        else if (inWord == 0)
        {
            inWord = 1;
            count++;
        }
    }
    return count;
}

float calculateAccuracy(char *original, char *typed)
{
    int correct = 0;
    int total = strlen(original);
    int minLen = (strlen(typed) < total) ? strlen(typed) : total;

    for (int i = 0; i < minLen; i++)
    {
        if (original[i] == typed[i])
            correct++;
    }
    return ((float)correct / total) * 100.0f;
}

// leaderboard + history helpers
void InitDefaultLeaderboard()
{
    int i;
    for (i = 0; i < LEADER_COUNT; i++)
    {
        leaderboard[i].name[0] = '\0';
        leaderboard[i].wpm = 0;
        leaderboard[i].accuracy = 0.0f;
    }

    // fixed benchmark entries (must never change except USER slot data)
    strncpy(leaderboard[0].name, "Pro C Coder", sizeof(leaderboard[0].name) - 1);
    leaderboard[0].wpm = 110;
    leaderboard[0].accuracy = 98.0f;

    strncpy(leaderboard[1].name, "Fast Writer", sizeof(leaderboard[1].name) - 1);
    leaderboard[1].wpm = 95;
    leaderboard[1].accuracy = 96.5f;

    strncpy(leaderboard[2].name, "Daily Typist", sizeof(leaderboard[2].name) - 1);
    leaderboard[2].wpm = 75;
    leaderboard[2].accuracy = 94.0f;

    // index 3 reserved for USER (only this slot may change)
    strncpy(leaderboard[3].name, "YOU", sizeof(leaderboard[3].name) - 1);
    leaderboard[3].wpm = 0;
    leaderboard[3].accuracy = 0.0f;

    strncpy(leaderboard[4].name, "Starter", sizeof(leaderboard[4].name) - 1);
    leaderboard[4].wpm = 40;
    leaderboard[4].accuracy = 90.0f;
}

void LoadLeaderboard()
{
    FILE *f;
    if (leaderboardLoaded)
        return;

    InitDefaultLeaderboard();

    f = fopen(LEADERBOARD_FILE, "r");
    if (f != NULL)
    {
        LeaderEntry tmp;
        if (fscanf(f, "%31[^\t]\t%d\t%f", tmp.name, &tmp.wpm, &tmp.accuracy) == 3)
        {
            strncpy(leaderboard[3].name, tmp.name, sizeof(leaderboard[3].name) - 1);
            leaderboard[3].wpm = tmp.wpm;
            leaderboard[3].accuracy = tmp.accuracy;
        }
        fclose(f);
    }

    leaderboardLoaded = 1;
}

void SaveUserLeaderboardEntry()
{
    FILE *f;
    f = fopen(LEADERBOARD_FILE, "w");
    if (f != NULL)
    {
        fprintf(f, "%s\t%d\t%.2f\n",
                leaderboard[3].name,
                leaderboard[3].wpm,
                leaderboard[3].accuracy);
        fclose(f);
    }
}

void AppendHistory()
{
    FILE *f;
    const char *modeName = "Unknown";
    if (selectedMode == 1)
        modeName = "Code";
    else if (selectedMode == 2)
        modeName = "Standard";
    else if (selectedMode == 3)
        modeName = "Sprint";

    f = fopen(HISTORY_FILE, "a");
    if (f != NULL)
    {
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char timeBuf[64];
        if (tm_info != NULL)
        {
            strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", tm_info);
        }
        else
        {
            strncpy(timeBuf, "unknown-time", sizeof(timeBuf) - 1);
            timeBuf[sizeof(timeBuf) - 1] = '\0';
        }

        fprintf(f,
                "%s\tMode:%s\tWPM:%d\tAccuracy:%.2f\tWords:%d\tTime:%d\tText:\"%s\"\tInput:\"%s\"\n",
                timeBuf,
                modeName,
                wpm,
                accuracy,
                totalWords,
                timeTaken,
                currentText ? currentText : "",
                userInput);
        fclose(f);
    }
}

// compare score of two entries: return 1 if a should rank above b
int CompareEntryScore(const LeaderEntry *a, const LeaderEntry *b)
{
    if (a->wpm > b->wpm)
        return 1;
    if (a->wpm < b->wpm)
        return 0;
    if (a->accuracy > b->accuracy)
        return 1;
    if (a->accuracy < b->accuracy)
        return 0;
    return 0;
}

// build ranking order (indices into leaderboard[])
void BuildDisplayOrder()
{
    int i, j;
    for (i = 0; i < LEADER_COUNT; i++)
        leaderboardOrder[i] = i;

    for (i = 0; i < LEADER_COUNT - 1; i++)
    {
        int best = i;
        for (j = i + 1; j < LEADER_COUNT; j++)
        {
            if (CompareEntryScore(&leaderboard[leaderboardOrder[j]],
                                  &leaderboard[leaderboardOrder[best]]))
            {
                best = j;
            }
        }
        if (best != i)
        {
            int tmp = leaderboardOrder[i];
            leaderboardOrder[i] = leaderboardOrder[best];
            leaderboardOrder[best] = tmp;
        }
    }
}

void UpdateUserSlotFromResult()
{
    if (wpm > leaderboard[3].wpm ||
        (wpm == leaderboard[3].wpm && accuracy > leaderboard[3].accuracy))
    {
        leaderboard[3].wpm = wpm;
        leaderboard[3].accuracy = accuracy;
        SaveUserLeaderboardEntry();
    }

    BuildDisplayOrder();
}

void Text()
{
    int index = rand() % 5;
    if (selectedMode == 1)
    {
        currentText = codeTexts[index];
    }
    else if (selectedMode == 2)
    {
        currentText = standardTexts[index];
    }
    else
    {
        currentText = sprintTexts[index];
    }
}

void Pre_Test()
{
    testActive = 1;
    startTime = time(NULL);
    inputlen = 0;
    userInput[0] = '\0';
    currentState = 1;
}

void Post_Test()
{
    testActive = 0;
    time_t endTime = time(NULL);
    timeTaken = (int)difftime(endTime, startTime);

    totalWords = countWords(userInput);
    wpm = (timeTaken > 0) ? (totalWords * 60) / timeTaken : 0;
    accuracy = calculateAccuracy(currentText, userInput);

    currentState = 2;

    LoadLeaderboard();
    UpdateUserSlotFromResult();
    AppendHistory();
}

int main(void)
{
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Typing Speed Test\nby Fireteam Forerunner");
    SetTargetFPS(60);

    srand(time(NULL));
    LoadLeaderboard();
    BuildDisplayOrder();

    // Colors (blue, black, white, red theme)
    Color bgColor = (Color){230, 240, 255, 255};       // very light blue background
    Color windowColor = (Color){255, 255, 255, 255};   // white main panel
    Color borderColor = (Color){30, 60, 120, 255};     // dark blue border
    Color textColor = (Color){0, 0, 0, 255};           // black text
    Color buttonColor = (Color){200, 220, 255, 255};   // soft blue button
    Color buttonHover = (Color){160, 190, 245, 255};   // stronger blue hover
    Color statsBoxColor = (Color){245, 250, 255, 255}; // very light blue for result boxes
    Color accentRed = (Color){200, 40, 40, 255};       // red accent for key labels / title

    while (!WindowShouldClose())
    {
        Vector2 mousePos = GetMousePosition();

        // Input handling for typing
        if (currentState == 1 && testActive)
        {
            int key = GetCharPressed();
            while (key > 0)
            {
                if (key >= 32 && key <= 125 && inputlen < MAX_INPUT - 1)
                {
                    userInput[inputlen] = (char)key;
                    inputlen++;
                    userInput[inputlen] = '\0';

                    if (inputlen >= strlen(currentText))
                    {
                        Post_Test();
                    }
                }
                key = GetCharPressed();
            }

            if (IsKeyPressed(KEY_BACKSPACE) && inputlen > 0)
            {
                inputlen--;
                userInput[inputlen] = '\0';
            }

            if (IsKeyPressed(KEY_ENTER))
            {
                Post_Test();
            }
        }

        BeginDrawing();
        ClearBackground(bgColor);

        // Draw main window
        DrawRectangle(50, 50, 800, 600, windowColor);
        DrawRectangleLines(50, 50, 800, 600, borderColor);

        if (currentState == 0)
        {
            // MODE SELECTION
            DrawText("TYPING SPEED TEST", 275, 85, 28, accentRed);
            DrawText("By Fireteam Forerunner", 295, 122, 21, textColor);
            DrawText("Select Test Mode:", 340, 165, 20, textColor);

            // Code Mode button
            Rectangle codeBtn = (Rectangle){100, 220, 200, 100};
            int codeHover = CheckCollisionPointRec(mousePos, codeBtn);
            DrawRectangleRec(codeBtn, codeHover ? buttonHover : buttonColor);
            DrawRectangleLinesEx(codeBtn, 1, borderColor);
            DrawText("Code Mode", 135, 250, 20, textColor);
            DrawText("Programming", 137, 280, 16, textColor);

            if (codeHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                selectedMode = 1;
                Text();
                Pre_Test();
            }

            // Standard Mode button
            Rectangle stdBtn = (Rectangle){350, 220, 200, 100};
            int stdHover = CheckCollisionPointRec(mousePos, stdBtn);
            DrawRectangleRec(stdBtn, stdHover ? buttonHover : buttonColor);
            DrawRectangleLinesEx(stdBtn, 1, borderColor);
            DrawText("Standard Mode", 365, 250, 20, textColor);
            DrawText("General Typing", 370, 280, 16, textColor);

            if (stdHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                selectedMode = 2;
                Text();
                Pre_Test();
            }

            // Sprint Mode button
            Rectangle sprBtn = (Rectangle){600, 220, 200, 100};
            int sprHover = CheckCollisionPointRec(mousePos, sprBtn);
            DrawRectangleRec(sprBtn, sprHover ? buttonHover : buttonColor);
            DrawRectangleLinesEx(sprBtn, 1, borderColor);
            DrawText("Sprint Mode", 630, 250, 20, textColor);
            DrawText("Max Speed", 635, 280, 16, textColor);

            if (sprHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                selectedMode = 3;
                Text();
                Pre_Test();
            }

            // Leaderboard button at mode selection
            Rectangle lbBtn = (Rectangle){350, 350, 200, 50};
            int lbHover = CheckCollisionPointRec(mousePos, lbBtn);
            DrawRectangleRec(lbBtn, lbHover ? buttonHover : buttonColor);
            DrawRectangleLinesEx(lbBtn, 1, borderColor);
            DrawText("Leaderboard", 380, 365, 20, textColor);
            if (lbHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                currentState = 3; // new view state
            }
        }
        else if (currentState == 1)
        {
            // TESTING
            int elapsed = (int)difftime(time(NULL), startTime);
            DrawText(TextFormat("Time: %02d:%02d", elapsed / 60, elapsed % 60), 380, 80, 24, accentRed);

            // Sample text
            DrawText("Sample Text:", 70, 140, 18, textColor);
            DrawRectangle(70, 170, 760, 100, statsBoxColor);
            DrawRectangleLines(70, 170, 760, 100, borderColor);
            DrawText(currentText, 80, 200, 20, textColor);

            // Input
            DrawText("Your Input:", 70, 300, 18, textColor);
            DrawRectangle(70, 330, 760, 100, statsBoxColor);
            DrawRectangleLines(70, 330, 760, 100, borderColor);
            DrawText(userInput, 80, 360, 20, textColor);

            // Cursor
            if ((elapsed % 2) == 0)
            {
                int cursorX = 80 + MeasureText(userInput, 20);
                DrawRectangle(cursorX, 360, 2, 20, accentRed);
            }

            DrawText("Type the text above. Press Enter when done.", 220, 460, 18, textColor);

            // Reset button
            Rectangle resetBtn = (Rectangle){380, 510, 140, 40};
            int resetHover = CheckCollisionPointRec(mousePos, resetBtn);
            DrawRectangleRec(resetBtn, resetHover ? buttonHover : buttonColor);
            DrawRectangleLinesEx(resetBtn, 1, borderColor);
            DrawText("Reset", 425, 520, 20, textColor);

            if (resetHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                testActive = 0;
                inputlen = 0;
                userInput[0] = '\0';
                Text();
                Pre_Test();
            }
        }
        else if (currentState == 2)
        {
            // RESULTS
            DrawText("TEST RESULTS", 320, 100, 28, accentRed);

            // Result boxes
            DrawRectangle(100, 180, 150, 120, statsBoxColor);
            DrawRectangleLines(100, 180, 150, 120, borderColor);
            DrawText(TextFormat("%d", wpm), 150, 210, 36, textColor);
            DrawText("WPM", 155, 260, 18, textColor);

            DrawRectangle(280, 180, 150, 120, statsBoxColor);
            DrawRectangleLines(280, 180, 150, 120, borderColor);
            DrawText(TextFormat("%.0f%%", accuracy), 315, 210, 36, textColor);
            DrawText("Accuracy", 305, 260, 18, textColor);

            DrawRectangle(460, 180, 150, 120, statsBoxColor);
            DrawRectangleLines(460, 180, 150, 120, borderColor);
            DrawText(TextFormat("%d", totalWords), 515, 210, 36, textColor);
            DrawText("Words", 505, 260, 18, textColor);

            DrawRectangle(640, 180, 150, 120, statsBoxColor);
            DrawRectangleLines(640, 180, 150, 120, borderColor);
            DrawText(TextFormat("%ds", timeTaken), 685, 210, 36, textColor);
            DrawText("Time", 685, 260, 18, textColor);

            // Feedback
            char *feedback = "Good job! Keep practicing!";
            if (selectedMode == 1 && accuracy >= 95)
                feedback = "Excellent accuracy with code!";
            else if (selectedMode == 3 && wpm >= 60)
                feedback = "Lightning fast speed!";
            else if (selectedMode == 2 && wpm >= 50 && accuracy >= 90)
                feedback = "Excellent performance!";

            DrawRectangle(150, 350, 600, 80, windowColor);
            DrawRectangleLines(150, 350, 600, 80, borderColor);
            DrawText(feedback, 200, 380, 20, textColor);

            // Leaderboard shown on results screen (ranked)
            DrawText("Leaderboard:", 100, 445, 18, textColor);
            {
                char line[128];
                int i;
                for (i = 0; i < LEADER_COUNT; i++)
                {
                    int idx = leaderboardOrder[i];
                    snprintf(line, sizeof(line), "%d. %s  -  %d WPM, %.1f%%",
                             i + 1,
                             leaderboard[idx].name,
                             leaderboard[idx].wpm,
                             leaderboard[idx].accuracy);
                    DrawText(line, 100, 470 + i * 18, 14, textColor);
                }
            }

            // Buttons
            Rectangle tryBtn = (Rectangle){250, 620, 140, 40};
            int tryHover = CheckCollisionPointRec(mousePos, tryBtn);
            DrawRectangleRec(tryBtn, tryHover ? buttonHover : buttonColor);
            DrawRectangleLinesEx(tryBtn, 1, borderColor);
            DrawText("Try Again", 270, 630, 18, textColor);

            if (tryHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                Text();
                Pre_Test();
            }

            Rectangle backBtn = (Rectangle){410, 620, 140, 40};
            int backHover = CheckCollisionPointRec(mousePos, backBtn);
            DrawRectangleRec(backBtn, backHover ? buttonHover : buttonColor);
            DrawRectangleLinesEx(backBtn, 1, borderColor);
            DrawText("Back", 460, 630, 18, textColor);

            if (backHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                currentState = 0;
                selectedMode = 0;
            }
        }
        else if (currentState == 3)
        {
            // LEADERBOARD VIEW (from mode selection)
            DrawText("LEADERBOARD", 330, 100, 28, accentRed);

            DrawRectangle(120, 160, 660, 360, windowColor);
            DrawRectangleLines(120, 160, 660, 360, borderColor);

            DrawText("Rank", 140, 180, 18, textColor);
            DrawText("Name", 200, 180, 18, textColor);
            DrawText("WPM", 440, 180, 18, textColor);
            DrawText("Accuracy", 560, 180, 18, textColor);

            {
                int i;
                char buf[64];
                for (i = 0; i < LEADER_COUNT; i++)
                {
                    int idx = leaderboardOrder[i];
                    snprintf(buf, sizeof(buf), "%d", i + 1);
                    DrawText(buf, 140, 210 + i * 30, 18, textColor);
                    DrawText(leaderboard[idx].name, 200, 210 + i * 30, 18, textColor);
                    snprintf(buf, sizeof(buf), "%d", leaderboard[idx].wpm);
                    DrawText(buf, 440, 210 + i * 30, 18, textColor);
                    snprintf(buf, sizeof(buf), "%.1f%%", leaderboard[idx].accuracy);
                    DrawText(buf, 560, 210 + i * 30, 18, textColor);
                }
            }

            Rectangle backLbBtn = (Rectangle){380, 540, 140, 40};
            int backLbHover = CheckCollisionPointRec(mousePos, backLbBtn);
            DrawRectangleRec(backLbBtn, backLbHover ? buttonHover : buttonColor);
            DrawRectangleLinesEx(backLbBtn, 1, borderColor);
            DrawText("Back", 430, 550, 18, textColor);

            if (backLbHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                currentState = 0;
            }
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
