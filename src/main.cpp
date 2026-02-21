#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>

using namespace std;

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

// --- ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ---

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
    
    if (visualLen > width) {
        return utf8_substr(text, 0, width - 1) + ".";
    }
    return text + string(width - visualLen, ' ');
}

string generateBar(int value) {
    string bar = "[";
    int filled = (value < 0) ? 0 : (value > 10 ? 10 : value);
    
    for (int i = 0; i < 10; i++) {
        if (i < filled) bar += "|";
        else bar += ".";
    }
    bar += "]";
    return bar;
}

// --- ОСНОВНАЯ ЛОГИКА ---

void loadHabits(const string& filename) {
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Ошибка: Не удалось открыть файл " << filename << endl;
        return;
    }

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
        } else {
             cerr << "Предупреждение: Строка настроек пропущена (мало полей): " << line << endl;
        }
    }
    file.close();
}

void loadJournal(const string& filename) {
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Ошибка: Не удалось открыть файл " << filename << endl;
        return;
    }

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

int main() {
    loadHabits("data/habits.cfg");
    loadJournal("data/journal.log");

    // --- ВЫВОД ШАПКИ ---
    // Ширина колонки "Сопротивление" увеличена до 14, чтобы влезло число и график
    cout << string(110, '-') << endl;
    
    // Заголовок
    cout << formatCell("День", 12) 
         << formatCell("Привычка (Ур.)", 20) 
         << formatCell("Триггер", 15)
         << formatCell("Действие", 16)
         << formatCell("Статус", 6)
         << formatCell("Сопротивление", 14) // Объединенная колонка
         << formatCell("Примечание", 20) 
         << endl;
         


    cout << string(110, '-') << endl;

    string currentDate = "";

    // --- ВЫВОД ДАННЫХ ---
    for (const auto& entry : journal) {
        
        // Вывод даты (группировка)
        if (currentDate != entry.date) {
            currentDate = entry.date;
            cout << "\n" << formatCell(currentDate, 12) << endl; 
        }

        if (habitsMap.find(entry.habitId) != habitsMap.end()) {
            HabitConfig habit = habitsMap[entry.habitId];

            // Первая строка: Основные данные + Число сопротивления
            cout << formatCell("", 12); 
            cout << formatCell(habit.name + " (L" + to_string(habit.level) + ")", 20);
            cout << formatCell(habit.trigger, 15);
            cout << formatCell(habit.action, 20);
            
            string statusStr(1, entry.status);
            cout << formatCell(statusStr, 6);
            
            // Выводим ЧИСЛО сопротивления
            cout << formatCell(to_string(entry.value), 14);
            
            // Примечание
            cout << formatCell(entry.note, 20) << endl;
            
            // Вторая строка: Пустые ячейки + ГРАФИК сопротивления
            cout << formatCell("", 12)
                 << formatCell("", 20)
                 << formatCell("", 15)
                 << formatCell("", 20)
                 << formatCell("", 6)
                 << formatCell(generateBar(entry.value), 14) // График под числом
                 << formatCell("", 20) << endl;

        } else {
            cout << formatCell("", 12) 
                 << formatCell("! ОШИБКА: ID " + to_string(entry.habitId) + " не найден", 50) << endl;
        }
    }

    cout << string(110, '-') << endl;
    cout << "\nНажмите Enter для выхода...";
    cin.get();

    return 0;
}