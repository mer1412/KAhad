#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <ctime>
#include <ncurses.h>
#include <locale.h> 

using namespace std;

// --- КОНСТАНТЫ ---
const string HABITS_FILE = "data/habits.cfg";
const string JOURNAL_FILE = "data/journal.log";

// --- СТРУКТУРЫ ---
struct HabitConfig { int id; int level; string name; string trigger; string action; bool active; };
struct LogEntry { string date; int habitId; char status; int value; string note; };

// --- ДАННЫЕ ---
map<int, HabitConfig> habitsMap;
vector<LogEntry> journal;

// --- ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ---

string getCurrentDate() {
    time_t now = time(0);
    tm* ltm = localtime(&now);
    char buffer[11];
    snprintf(buffer, sizeof(buffer), "%02d.%02d.%04d", ltm->tm_mday, 1 + ltm->tm_mon, 1900 + ltm->tm_year);
    return string(buffer);
}

vector<string> split(const string& s, char delimiter) {
    vector<string> tokens;
    size_t start = 0, end = 0;
    while ((end = s.find(delimiter, start)) != string::npos) {
        tokens.push_back(s.substr(start, end - start));
        start = end + 1;
    }
    tokens.push_back(s.substr(start));
    return tokens;
}

size_t utf8_strlen(const string& str) {
    size_t len = 0;
    for (size_t i = 0; i < str.length(); ++i) {
        if ((str[i] & 0xC0) != 0x80) len++;
    }
    return len;
}

string utf8_substr(const string& str, size_t start, size_t len) {
    size_t byteStart = 0; size_t charCount = 0;
    while (byteStart < str.length() && charCount < start) {
        if ((str[byteStart] & 0xC0) != 0x80) charCount++; byteStart++;
    }
    size_t byteEnd = byteStart; charCount = 0;
    while (byteEnd < str.length() && charCount < len) {
        if ((str[byteEnd] & 0xC0) != 0x80) charCount++; byteEnd++;
    }
    return str.substr(byteStart, byteEnd - byteStart);
}

string formatCell(string text, int width) {
    size_t visualLen = utf8_strlen(text);
    if (visualLen > width) return utf8_substr(text, 0, width - 1) + ".";
    return text + string(width - visualLen, ' ');
}

string generateBar(int value) {
    string bar = "[";
    int filled = (value < 0) ? 0 : (value > 10 ? 10 : value);
    for (int i = 0; i < 10; i++) {
        if (i < filled) bar += "|"; else bar += ".";
    }
    bar += "]";
    return bar;
}

// --- ФУНКЦИИ ДИАЛОГОВ ---

// Возвращает строку. Если пользователь ввел 'q', возвращаем пустую строку (сигнал отмены).
// isCancelAllowed: true = режим "q для выхода", false = обязательный ввод.
string ncursesInput(int row, int col, const string& prompt, bool isCancelAllowed = true) {
    echo(); 
    curs_set(1); 
    
    mvprintw(row, col, prompt.c_str());
    clrtoeol(); // Очищаем остаток строки
    
    char input[100];
    getnstr(input, 99); 
    
    noecho(); 
    curs_set(0);

    string result(input);
    
    // Если разрешен выход и ввели q
    if (isCancelAllowed && result == "q") {
        return ""; // Сигнал отмены
    }
    return result;
}

// Возвращает true, если число успешно считано. false, если отмена.
// Использует цикл для защиты от неверного ввода.
bool ncursesInputInt(int& var, int row, int col, const string& prompt) {
    while (true) {
        string s = ncursesInput(row, col, prompt, true);
        
        if (s.empty()) return false; // Нажали q (отмена)

        try {
            size_t pos;
            int val = stoi(s, &pos);
            // Проверяем, что вся строка была числом (чтобы отсеять "123abc")
            if (pos == s.length()) {
                var = val;
                return true;
            }
        } catch (...) {
            // Ошибка преобразования
        }
        
        // Если мы здесь, ввод некорректен
        mvprintw(row + 1, col, "Ошибка: введите корректное число!");
        clrtoeol();
    }
}

// --- ЗАГРУЗКА/СОХРАНЕНИЕ ---

void saveHabits() {
    ofstream file(HABITS_FILE);
    if (!file.is_open()) return;
    for (const auto& pair : habitsMap) {
        const HabitConfig& h = pair.second;
        file << h.id << "|" << h.level << "|" << h.name << "|" << h.trigger << "|" << h.action << "|" << h.active << endl;
    }
    file.close();
}

void saveJournal() {
    ofstream file(JOURNAL_FILE);
    if (!file.is_open()) return;
    for (const auto& e : journal) {
        file << e.date << "|" << e.habitId << "|" << e.status << "|" << e.value << "|" << e.note << endl;
    }
    file.close();
}

void loadHabits() {
    ifstream file(HABITS_FILE);
    if (!file.is_open()) return;
    string line;
    while (getline(file, line)) {
        if (line.empty()) continue;
        vector<string> tokens = split(line, '|');
        if (tokens.size() >= 6) {
            HabitConfig h;
            h.id = stoi(tokens[0]); h.level = stoi(tokens[1]); h.name = tokens[2];
            h.trigger = tokens[3]; h.action = tokens[4]; h.active = (stoi(tokens[5]) != 0);
            habitsMap[h.id] = h;
        }
    }
    file.close();
}

void loadJournal() {
    ifstream file(JOURNAL_FILE);
    if (!file.is_open()) return;
    string line;
    while (getline(file, line)) {
        if (line.empty()) continue;
        vector<string> tokens = split(line, '|');
        if (tokens.size() >= 5) {
            LogEntry e;
            e.date = tokens[0]; e.habitId = stoi(tokens[1]); e.status = tokens[2][0];
            e.value = stoi(tokens[3]); e.note = tokens[4];
            journal.push_back(e);
        }
    }
    file.close();
}

// --- ЭКРАНЫ (SCREENS) ---

void showDashboard() {
    int ch;
    int offset = 0;
    int maxRows = LINES - 4;

    while (true) {
        clear();
        
        attron(A_BOLD);
        mvprintw(0, 2, "DASHBOARD | Стрелки Вверх/Вниз | q - Выход в меню");
        attroff(A_BOLD);
        
        int line = 2;
        
        mvprintw(line, 0, formatCell("Дата", 12).c_str());
        printw(formatCell("Привычка", 20).c_str());
        printw(formatCell("Статус", 6).c_str());
        printw(formatCell("Сопр.", 14).c_str());
        printw(formatCell("Примечание", 20).c_str());
        
        line++;
        
        int displayed = 0;
        for (size_t i = offset; i < journal.size() && displayed < maxRows; i++) {
            const auto& entry = journal[i];
            
            if (i == 0 || journal[i-1].date != entry.date) {
                attron(A_DIM);
                mvprintw(line, 0, formatCell(entry.date, 12).c_str());
                attroff(A_DIM);
            } else {
                mvprintw(line, 0, formatCell("", 12).c_str());
            }

            if (habitsMap.find(entry.habitId) != habitsMap.end()) {
                HabitConfig habit = habitsMap[entry.habitId];
                printw(formatCell(habit.name + " (L" + to_string(habit.level) + ")", 20).c_str());
                printw(formatCell(string(1, entry.status), 6).c_str());
                
                if (entry.value > 7) attron(COLOR_PAIR(1)); 
                else if (entry.value > 3) attron(COLOR_PAIR(2)); 
                else attron(COLOR_PAIR(3));

                printw(formatCell(to_string(entry.value) + " " + generateBar(entry.value), 14).c_str());
                
                attroff(COLOR_PAIR(1)); attroff(COLOR_PAIR(2)); attroff(COLOR_PAIR(3));
                
                printw(formatCell(entry.note, 20).c_str());
            }
            
            line++;
            displayed++;
        }

        mvprintw(LINES-1, 2, "Записей: %zu", journal.size());

        ch = getch();
        if (ch == 'q' || ch == 27) break;
        if (ch == KEY_UP && offset > 0) offset--;
        if (ch == KEY_DOWN && offset < (int)journal.size() - maxRows) offset++;
    }
}

void listHabits() {
    int ch;
    int highlight = 0;
    vector<int> ids;
    for(auto& p : habitsMap) ids.push_back(p.first);

    while(true) {
        clear();
        attron(A_BOLD);
        mvprintw(0, 2, "СПИСОК ПРИВЫЧЕК | q - Выход");
        attroff(A_BOLD);

        int line = 2;
        if (ids.empty()) {
             mvprintw(line, 2, "Список пуст. Нажмите '3' для добавления.");
        } else {
            for (size_t i = 0; i < ids.size(); i++) {
                HabitConfig h = habitsMap[ids[i]];
                
                if (i == highlight) {
                    attron(A_REVERSE);
                    mvprintw(line, 0, "-> ");
                } else {
                    mvprintw(line, 0, "   ");
                }

                printw("ID:%d | Lvl:%d | %s | %s | %s", 
                       h.id, h.level, 
                       formatCell(h.name, 20).c_str(), 
                       (h.active ? "[ON]" : "[OFF]"), 
                       h.trigger.c_str());
                
                if (i == highlight) attroff(A_REVERSE);
                line++;
            }
        }
        
        mvprintw(LINES-1, 2, "3 - Добавить | 5 - Вкл/Выкл | 6 - Удалить");
        ch = getch();

        if (ch == 'q' || ch == 27) break;
        
        // Навигация с защитой от пустого списка
        if (!ids.empty()) {
            if (ch == KEY_UP && highlight > 0) highlight--;
            if (ch == KEY_DOWN && highlight < ids.size() - 1) highlight++;
            
            if (ch == '5') {
                habitsMap[ids[highlight]].active = !habitsMap[ids[highlight]].active;
                saveHabits();
            }
            if (ch == '6') {
                mvprintw(LINES-2, 2, "Удалить? (y/n): ");
                if(getch() == 'y') {
                    habitsMap.erase(ids[highlight]);
                    ids.erase(ids.begin() + highlight);
                    saveHabits();
                    if (highlight > 0) highlight--;
                }
            }
        }
        
        if (ch == '3') {
            // Логика добавления перенесена в отдельную функцию для чистоты, 
            // но здесь мы просто выйдем из цикла списка и меню само перерисуется
            // Для простоты в этом меню мы не будем вызывать addHabit здесь,
            // чтобы не плодить вложенность, лучше пользоваться главным меню.
            // Но если нужно, можно вызвать:
            // addHabit(); 
            // ids.clear(); for(auto& p : habitsMap) ids.push_back(p.first); // Обновить список
        }
    }
}

void addHabit() {
    clear();
    curs_set(1); echo();
    
    HabitConfig h;
    h.id = 0;
    for (const auto& pair : habitsMap) if (pair.first > h.id) h.id = pair.first;
    h.id++; 
    h.level = 1; h.active = true;

    mvprintw(2, 2, "--- Создание привычки (ID: %d) ---", h.id);
    mvprintw(LINES-2, 2, "Введите 'q' в любом поле для отмены.");
    
    // Ввод названия
    h.name = ncursesInput(4, 2, "Название: ");
    if (h.name.empty()) { curs_set(0); noecho(); return; } // Отмена
    
    h.trigger = ncursesInput(5, 2, "Триггер: ");
    if (h.trigger.empty()) { curs_set(0); noecho(); return; }
    
    h.action = ncursesInput(6, 2, "Действие: ");
    if (h.action.empty()) { curs_set(0); noecho(); return; }
    
    // Ввод уровня с валидацией
    // Предлагаем значение по умолчанию, если просто нажать Enter?
    // Для упрощения запросим ввод, но разрешим 'q'.
    if (!ncursesInputInt(h.level, 7, 2, "Уровень (1): ")) {
        curs_set(0); noecho(); return; // Отмена
    }
    if (h.level < 1) h.level = 1;

    habitsMap[h.id] = h;
    saveHabits();
    
    mvprintw(9, 2, "Привычка добавлена! Нажмите любую клавишу...");
    getch();
    
    curs_set(0); noecho();
}

void addJournalEntry() {
    clear();
    curs_set(1); echo();
    
    LogEntry e;
    
    mvprintw(2, 2, "--- Новая запись ---");
    mvprintw(LINES-2, 2, "Введите 'q' в любом поле для отмены.");
    
    mvprintw(4, 2, "1. Сегодня (%s)", getCurrentDate().c_str());
    mvprintw(5, 2, "2. Другая дата");
    
    int choice = 0;
    // Защищенный ввод выбора меню (1 или 2)
    while (true) {
        if (!ncursesInputInt(choice, 7, 2, "Выбор: ")) {
            curs_set(0); noecho(); return; // Нажали q
        }
        if (choice == 1 || choice == 2) break;
        mvprintw(8, 2, "Ошибка: введите 1 или 2.");
    }

    if (choice == 1) {
        e.date = getCurrentDate();
    } else {
        e.date = ncursesInput(8, 2, "Дата (ДД.ММ.ГГГГ): ");
        if (e.date.empty()) { curs_set(0); noecho(); return; }
    }

    int line = 10;
    mvprintw(line, 2, "Активные привычки:");
    line+=2;
    
    vector<int> activeIds;
    for (const auto& pair : habitsMap) {
        if (pair.second.active) {
            activeIds.push_back(pair.first);
            mvprintw(line, 2, "%d. %s", pair.first, pair.second.name.c_str());
            line++;
        }
    }
    
    if (activeIds.empty()) {
        mvprintw(line, 2, "Нет активных привычек! Нажмите кнопку...");
        getch();
        curs_set(0); noecho();
        return;
    }

    // Ввод ID привычки
    if (!ncursesInputInt(e.habitId, line+1, 2, "ID привычки: ")) {
        curs_set(0); noecho(); return;
    }
    
    // Проверка существования ID
    if (habitsMap.find(e.habitId) == habitsMap.end()) {
        mvprintw(line+2, 2, "Ошибка: ID не найден! Нажмите кнопку...");
        getch();
        curs_set(0); noecho();
        return;
    }

    string st = ncursesInput(line+2, 2, "Статус (+,-,*): ");
    if (st.empty()) { curs_set(0); noecho(); return; }
    e.status = st[0];
    
    if (!ncursesInputInt(e.value, line+3, 2, "Сопротивление (0-10): ")) {
        curs_set(0); noecho(); return;
    }
    
    // Примечание: здесь 'q' тоже отменит, но это нормально.
    e.note = ncursesInput(line+4, 2, "Примечание: ");
    // Для примечания пустая строка (Enter сразу) допустима, это не отмена.
    // Но если ввести 'q', отменит. Это компромисс.
    // Чтобы разрешить пустое примечание и иметь выход:
    // Можно ввести спец символ или изменить логику ncursesInput, 
    // но для простоты оставим как есть: 'q' = отмена всего.
    
    journal.push_back(e);
    saveJournal();
    
    mvprintw(line+6, 2, "Запись добавлена! Нажмите любую клавишу...");
    getch();
    
    curs_set(0); noecho();
}

// --- ГЛАВНОЕ МЕНЮ ---

int main() {
    setlocale(LC_ALL, "");

    initscr();
    start_color();
    init_pair(1, COLOR_RED, COLOR_BLACK);
    init_pair(2, COLOR_YELLOW, COLOR_BLACK);
    init_pair(3, COLOR_GREEN, COLOR_BLACK);
    
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    
    loadHabits();
    loadJournal();

    string choices[] = {
        "Показать Dashboard",
        "Список привычек",
        "Добавить привычку",
        "Добавить запись в журнал",
        "Выход"
    };
    int n_choices = 5;
    int highlight = 0;
    int ch;

    while (true) {
        clear();
        
        attron(A_BOLD);
        mvprintw(0, (COLS - 20)/2, "HABIT TRACKER v1.1");
        attroff(A_BOLD);
        
        for (int i = 0; i < n_choices; i++) {
            int x = (COLS - 30) / 2;
            int y = 3 + i * 2;
            
            if (i == highlight) {
                attron(A_REVERSE);
                mvprintw(y, x, "-> %s", choices[i].c_str());
                attroff(A_REVERSE);
            } else {
                mvprintw(y, x+3, "%s", choices[i].c_str());
            }
        }
        
        ch = getch();
        switch(ch) {
            case KEY_UP:
                if (highlight > 0) highlight--;
                break;
            case KEY_DOWN:
                if (highlight < n_choices - 1) highlight++;
                break;
            case 10: // Enter
                if (highlight == 0) showDashboard();
                else if (highlight == 1) listHabits();
                else if (highlight == 2) addHabit();
                else if (highlight == 3) addJournalEntry();
                else if (highlight == 4) {
                    endwin();
                    return 0;
                }
                break;
            case 'q':
                 if (highlight == 4) {
                    endwin();
                    return 0;
                 }
                 break;
        }
    }

    endwin();
    return 0;
}