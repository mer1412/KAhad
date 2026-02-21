#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <ctime> // Для работы с датой

using namespace std;

// --- КОНСТАНТЫ ---
const string HABITS_FILE = "data/habits.cfg";
const string JOURNAL_FILE = "data/journal.log";

// --- СТРУКТУРЫ ДАННЫХ ---
struct HabitConfig {
    int id;
    int level;
    string name;
    string trigger;
    string action;
    bool active;
};

struct LogEntry {
    string date;
    int habitId;
    char status;  
    int value;    
    string note;
};

// --- ГЛОБАЛЬНЫЕ ДАННЫЕ ---
map<int, HabitConfig> habitsMap;
vector<LogEntry> journal;

// --- ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ВВОДА ---

// Безопасный ввод целого числа.
// Возвращает true, если число успешно считано. false, если пользователь ввел 'q' (отмена).
bool safeInputInt(int& var, const string& prompt) {
    string buffer;
    while (true) {
        cout << prompt << " (или 'q' для отмены): ";
        cin >> buffer;

        if (buffer == "q" || buffer == "Q") {
            return false; // Отмена операции
        }

        try {
            // Пытаемся преобразовать строку в число
            var = stoi(buffer);
            return true; // Успех
        } catch (...) {
            cout << "Ошибка: нужно ввести число. Попробуйте снова.\n";
        }
    }
}

// Ввод строки (позволяет оставлять пустым для сохранения старого значения)
string inputString(const string& prompt) {
    cout << prompt;
    string s;
    // Используем cin.ignore() только если перед этим был cin >>
    if (cin.peek() == '\n') cin.ignore(); 
    getline(cin, s);
    return s;
}

// --- ДАТА И СТРОКИ ---

string getCurrentDate() {
    time_t now = time(0);
    tm* ltm = localtime(&now);
    char buffer[11];
    snprintf(buffer, sizeof(buffer), "%02d.%02d.%04d", ltm->tm_mday, 1 + ltm->tm_mon, 1900 + ltm->tm_year);
    return string(buffer);
}

vector<string> split(const string& s, char delimiter) {
    vector<string> tokens;
    string token;
    size_t start = 0, end = 0;
    while ((end = s.find(delimiter, start)) != string::npos) {
        tokens.push_back(s.substr(start, end - start));
        start = end + 1;
    }
    tokens.push_back(s.substr(start));
    return tokens;
}

// --- UTF-8 ФОРМАТИРОВАНИЕ ---

size_t utf8_strlen(const string& str) {
    size_t len = 0;
    for (size_t i = 0; i < str.length(); ++i) {
        if ((str[i] & 0xC0) != 0x80) len++;
    }
    return len;
}

string utf8_substr(const string& str, size_t start, size_t len) {
    size_t byteStart = 0;
    size_t charCount = 0;
    while (byteStart < str.length() && charCount < start) {
        if ((str[byteStart] & 0xC0) != 0x80) charCount++;
        byteStart++;
    }
    size_t byteEnd = byteStart;
    charCount = 0;
    while (byteEnd < str.length() && charCount < len) {
        if ((str[byteEnd] & 0xC0) != 0x80) charCount++;
        byteEnd++;
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

// --- ФУНКЦИИ СОХРАНЕНИЯ И ЗАГРУЗКИ ---

void saveHabits() {
    ofstream file(HABITS_FILE);
    if (!file.is_open()) {
        cout << "Ошибка сохранения настроек!\n";
        return;
    }
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
            h.id = stoi(tokens[0]);
            h.level = stoi(tokens[1]);
            h.name = tokens[2];
            h.trigger = tokens[3];
            h.action = tokens[4];
            h.active = (stoi(tokens[5]) != 0);
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
            e.date = tokens[0];
            e.habitId = stoi(tokens[1]);
            e.status = tokens[2][0]; 
            e.value = stoi(tokens[3]);
            e.note = tokens[4];
            journal.push_back(e);
        }
    }
    file.close();
}

// --- ИНТЕРФЕЙС (UI) ---

void showDashboard() {
    cout << string(110, '-') << endl;
    cout << formatCell("День", 12) 
         << formatCell("Привычка (Ур.)", 20) 
         << formatCell("Триггер", 15)
         << formatCell("Действие", 20)
         << formatCell("Статус", 6)
         << formatCell("Сопротивление", 14)
         << formatCell("Примечание", 20) 
         << endl;
    cout << formatCell("", 12) << formatCell("", 20) << formatCell("", 15) 
         << formatCell("", 20) << formatCell("", 6) << formatCell("(число [|||..])", 14) << endl;
    cout << string(110, '-') << endl;

    string currentDate = "";
    for (const auto& entry : journal) {
        if (currentDate != entry.date) {
            currentDate = entry.date;
            cout << "\n" << formatCell(currentDate, 12) << endl; 
        }

        if (habitsMap.find(entry.habitId) != habitsMap.end()) {
            HabitConfig habit = habitsMap[entry.habitId];
            cout << formatCell("", 12); 
            cout << formatCell(habit.name + " (L" + to_string(habit.level) + ")", 20);
            cout << formatCell(habit.trigger, 15);
            cout << formatCell(habit.action, 20);
            cout << formatCell(string(1, entry.status), 6);
            cout << formatCell(to_string(entry.value), 14);
            cout << formatCell(entry.note, 20) << endl;
            cout << formatCell("", 12) << formatCell("", 20) << formatCell("", 15)
                 << formatCell("", 20) << formatCell("", 6) 
                 << formatCell(generateBar(entry.value), 14) << endl;
        }
    }
    cout << string(110, '-') << endl;
}

void listHabits() {
    cout << "\n--- Список привычек ---\n";
    cout << formatCell("ID", 4) << formatCell("Акт", 4) << formatCell("Ур", 3) << formatCell("Название", 20) << formatCell("Триггер", 15) << endl;
    cout << string(60, '-') << endl;
    for (const auto& pair : habitsMap) {
        const HabitConfig& h = pair.second;
        cout << formatCell(to_string(h.id), 4) 
             << formatCell(h.active ? "+" : "-", 4) 
             << formatCell(to_string(h.level), 3) 
             << formatCell(h.name, 20) 
             << formatCell(h.trigger, 15) << endl;
    }
    cout << endl;
}

void addHabit() {
    HabitConfig h;
    h.id = 0;
    if (!habitsMap.empty()) {
        for (const auto& pair : habitsMap) if (pair.first > h.id) h.id = pair.first;
    }
    h.id++; 
    
    h.level = 1; // Значение по умолчанию
    h.active = true;

    cout << "\nСоздание новой привычки (ID: " << h.id << ")\n";
    
    // Для строк 'q' не сработает как отмена в inputString, добавим логику
    h.name = inputString("Название: ");
    if (h.name == "q") { cout << "Отмена.\n"; return; }
    
    h.trigger = inputString("Триггер: ");
    if (h.trigger == "q") { cout << "Отмена.\n"; return; }
    
    h.action = inputString("Действие: ");
    if (h.action == "q") { cout << "Отмена.\n"; return; }

    // Безопасный ввод числа с возможностью отмены
    if (!safeInputInt(h.level, "Уровень (по умолч 1)")) {
        cout << "Отмена операции.\n";
        return;
    }
    if (h.level < 1) h.level = 1;

    habitsMap[h.id] = h;
    saveHabits();
    cout << "Привычка '" << h.name << "' добавлена!\n";
}

void editHabit() {
    listHabits();
    int id;
    if (!safeInputInt(id, "Введите ID привычки для редактирования")) {
        cout << "Отмена.\n";
        return;
    }

    if (habitsMap.find(id) != habitsMap.end()) {
        HabitConfig h = habitsMap[id];
        cout << "Редактирование: " << h.name << " (Оставьте пустым, чтобы не менять, 'q' - выход)\n";
        
        string input;
        
        input = inputString("Новое название (" + h.name + "): ");
        if (input == "q") { cout << "Отмена.\n"; return; }
        if (!input.empty()) h.name = input;

        input = inputString("Новый триггер (" + h.trigger + "): ");
        if (input == "q") { cout << "Отмена.\n"; return; }
        if (!input.empty()) h.trigger = input;

        input = inputString("Новое действие (" + h.action + "): ");
        if (input == "q") { cout << "Отмена.\n"; return; }
        if (!input.empty()) h.action = input;

        // Для чисел используем особый ввод
        int newLevel;
        if (!safeInputInt(newLevel, "Новый уровень (" + to_string(h.level) + ")")) {
            cout << "Отмена.\n";
            return;
        }
        h.level = newLevel;

        habitsMap[id] = h;
        saveHabits();
        cout << "Сохранено!\n";
    } else {
        cout << "ID не найден!\n";
    }
}

void toggleHabit() {
    listHabits();
    int id;
    if (!safeInputInt(id, "Введите ID привычки для включения/выключения")) {
        cout << "Отмена.\n";
        return;
    }

    if (habitsMap.find(id) != habitsMap.end()) {
        habitsMap[id].active = !habitsMap[id].active;
        saveHabits();
        cout << "Статус изменен: " << (habitsMap[id].active ? "ВКЛ" : "ВЫКЛ") << endl;
    } else {
        cout << "ID не найден!\n";
    }
}

void deleteHabit() {
    listHabits();
    int id;
    if (!safeInputInt(id, "Введите ID привычки для УДАЛЕНИЯ")) {
        cout << "Отмена удаления.\n";
        return;
    }

    if (habitsMap.find(id) != habitsMap.end()) {
        string name = habitsMap[id].name;
        // Дополнительное подтверждение
        string confirm;
        cout << "Вы уверены, что хотите удалить '" << name << "'? (y/n): ";
        cin >> confirm;
        if (confirm == "y" || confirm == "Y") {
            habitsMap.erase(id);
            saveHabits();
            cout << "Привычка '" << name << "' удалена.\n";
        } else {
            cout << "Отмена удаления.\n";
        }
    } else {
        cout << "ID не найден!\n";
    }
}

void addJournalEntry() {
    LogEntry e;
    
    cout << "\n--- Новая запись в журнале ---\n";
    cout << "1. Сегодня (" << getCurrentDate() << ")\n";
    cout << "2. Ввести дату вручную\n";
    
    int choice;
    if (!safeInputInt(choice, "Выбор")) {
        cout << "Отмена.\n"; return;
    }
    
    if (choice == 1) {
        e.date = getCurrentDate();
    } else {
        cout << "Дата (ДД.ММ.ГГГГ): ";
        cin >> e.date;
    }

    cout << "\nВыберите привычку (введите ID):\n";
    for (const auto& pair : habitsMap) {
        if (pair.second.active) {
            cout << "  " << pair.first << ". " << pair.second.name << endl;
        }
    }
    
    int idInput;
    if (!safeInputInt(idInput, "ID привычки")) {
        cout << "Отмена.\n"; return;
    }
    e.habitId = idInput;

    if (habitsMap.find(e.habitId) == habitsMap.end()) {
        cout << "Ошибка: Неверный ID привычки.\n";
        return;
    }

    cout << "Статус (+, -, *): ";
    cin >> e.status;
    
    int valInput;
    if (!safeInputInt(valInput, "Сопротивление (0-10)")) {
        cout << "Отмена.\n"; return;
    }
    e.value = valInput;
    
    cin.ignore(); // Очистка перед вводом строки
    cout << "Примечание: ";
    getline(cin, e.note);

    journal.push_back(e);
    saveJournal();
    cout << "Запись добавлена!\n";
}

// --- ГЛАВНОЕ МЕНЮ ---

int main() {
    loadHabits();
    loadJournal();

    int choice = 0;
    while (true) {
        cout << "\n=== TRACKER MENU ===\n";
        cout << "1. Показать Dashboard\n";
        cout << "2. Список привычек\n";
        cout << "3. Добавить привычку\n";
        cout << "4. Редактировать привычку\n";
        cout << "5. Вкл/Выкл привычку\n";
        cout << "6. Удалить привычку\n";
        cout << "7. Добавить запись в журнал\n";
        cout << "9. Выход\n";
        
        // Используем наш безопасный ввод для меню тоже
        if (!safeInputInt(choice, "Выбор")) {
            continue; // Если ввели q, просто показываем меню снова
        }

        switch (choice) {
            case 1: showDashboard(); break;
            case 2: listHabits(); break;
            case 3: addHabit(); break;
            case 4: editHabit(); break;
            case 5: toggleHabit(); break;
            case 6: deleteHabit(); break;
            case 7: addJournalEntry(); break;
            case 9: cout << "Выход...\n"; return 0;
            default: cout << "Неверный ввод.\n"; break;
        }
    }

    return 0;
}