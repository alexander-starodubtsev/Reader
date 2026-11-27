# Архитектура проекта: Синхронизация книги и аудиокниги (BookAudioSync)

## 1. Структура файлов и папок в проекте

Для чистоты кода разделим проект на модули. Каждый модуль — это пара файлов `.h`
(заголовочный) и `.cpp` (реализация).

```text
BookAudioSync/
├── CMakeLists.txt              # Сборка проекта (например, через CMake)
├── include/                    # Внешние библиотеки (nlohmann/json, sqlite3 и т.д.)
├── src/
│   ├── main.cpp                # Точка входа, инициализация и CLI-интерфейс
│   ├── AudioSource.h/.cpp      # Управление аудиофайлами, курсором и STT
│   ├── TextNormalizer.h/.cpp   # Очистка текста, токенизация, триграммы
│   ├── TextCorpus.h/.cpp       # Работа со SQLite (хранение слов, оригинал/нормализация)
│   ├── TextSearcher.h/.cpp     # Поиск по триграммам с учетом монотонного окна
│   ├── AnchorManager.h/.cpp    # Управление якорями и линейная интерполяция
│   └── SyncEngine.h/.cpp       # Главный координатор (связывает аудио, текст и UI)
└── data/
    └── book_database.sqlite    # Локальная база данных книги

```

---

## 2. Структура Базы Данных (SQLite)

Вся текстовая информация, поисковые индексы и якоря хранятся в SQLite для быстрого доступа
без загрузки всей книги в оперативную память.

### Таблица `words` (Слова книги)

* `word_id` (INTEGER, PRIMARY KEY) — сквозной номер слова от начала книги (0, 1, 2...).
* `original_form` (TEXT) — исходное слово (для отрисовки пользователю: с запятыми,
регистром и т.д.).
* `normalized_form` (TEXT) — очищенное слово (без знаков препинания, в нижнем регистре —
для поиска).
* `chapter_id` (INTEGER) — номер главы.
* `page_number` (INTEGER) — номер страницы, на которой встречается слово.

### Таблица `ngrams_index` (Поисковый индекс триграмм)

* `ngram_hash` (INTEGER или TEXT) — хэш от комбинации 3 нормализованных слов подряд.
* `word_id` (INTEGER) — позиция (слово), с которой начинается эта триграмма в книге.
* *(Индекс строится по колонке `ngram_hash` для мгновенного поиска за $O(1)$)*.

### Таблица `anchors` (Якоря)

* `time_sec` (REAL) — глобальное время в аудиокниге (в секундах).
* `word_id` (INTEGER) — соответствующий индекс слова в книге.

---

## 3. Описание классов и их методов

### Модуль 1: `TextNormalizer` (Утилитарный класс / набор функций)

Отвечает за подготовку текста к одинаковому виду как для книги, так и для STT.

* **Методы:**
* `std::string normalizeWord(const std::string& raw_word)` — приводит слово к нижнему
регистру, удаляет знаки препинания.
* `std::vector<std::string> tokenize(const std::string& text)` — разбивает текст на массив
отдельных нормализованных слов.
* `std::vector<uint64_t> generateNgrams(const std::vector<std::string>& words, size_t n =
3)` — генерирует хэши триграмм из массива слов.



### Модуль 2: `TextCorpus` (Работа с базой данных)

Менеджер хранения книги в SQLite.

* **Методы:**
* `bool open(const std::string& db_path)` — подключение к SQLite.
* `void importBook(const std::string& book_file)` — парсинг исходного файла книги, прогон
через `TextNormalizer` и массовая запись в БД (`words` + `ngrams_index`).
* `std::string getOriginalWord(size_t word_id) const` — достать слово для экрана.
* `size_t getTotalWords() const` — общее число слов в книге.
* `std::vector<size_t> findNgramMatches(uint64_t ngram_hash) const` — найти все `word_id`
по хэшу триграммы.



### Модуль 3: `AudioSource` (Управление аудио и STT)

Управляет списком аудиофайлов главы/книги, движением курсора и вызовом STT-модуля.

* **Методы:**
* `bool load(const std::vector<std::string>& audio_filepaths)` — инициализация списка
аудиофайлов и расчет их длительностей.
* `void seek(double global_sec)` — переместить курсор на заданную секунду (с учетом
разделения на файлы).
* `void seekRelative(double delta_sec)` — сместить курсор вперед/назад.
* `double getCursor() const` — получить текущее глобальное время.
* `std::string recognizeChunk(double duration_sec)` — записать/обработать `duration_sec`
аудио с текущей позиции и вернуть распознанный текст (через STT, например, Whisper.cpp).



### Модуль 4: `AnchorManager` (Якоря и интерполяция)

Хранит пары «время $\leftrightarrow$ слово» и строит по ним предсказания.

* **Методы:**
* `void addAnchor(double time_sec, size_t word_id)` — добавить якорь.
* `size_t estimateWordForTime(double time_sec) const` — линейная интерполяция: зная время,
прикинуть примерный `word_id` по ближайшим якорям.
* `double estimateTimeForWord(size_t word_id) const` — обратная интерполяция: зная
`word_id`, прикинуть время.
* `void saveToJson(const std::string& filepath) const` — сохранить якоря на диск.
* `bool loadFromJson(const std::string& filepath)` — загрузить якоря с диска.



### Модуль 5: `TextSearcher` (Умный поиск с оптимизациями)

Ищет точное положение STT-текста в книге, используя оптимизации.

* **Методы:**
* `size_t findPosition(const std::string& stt_text, size_t hint_word_id)` — главный метод
поиска.
* *Оптимизация 1 (Монотонное окно):* Ограничивает область поиска в БД диапазоном
`[hint_word_id - 500 ... hint_word_id + 2000]`.
* *Оптимизация 2 (Фильтрация шума):* Отсекает слишком частые триграммы (стоп-слова).
* *Оптимизация 3 (Ленточный Левенштейн):* Внутри суженного диапазона производит точное
нечеткое сравнение строк и возвращает искомый `word_id`.





### Модуль 6: `SyncEngine` (Главный координатор)

Связывает все модули воедино. Управляет логикой чтения глазами и прослушивания.

* **Методы:**
* `void initialize(const std::vector<std::string>& audio_files, const std::string&
book_file)` — старт системы.
* `void generateInitialAnchors(double interval_sec)` — фоновый процесс создания первичных
якорей (каждые 10 минут распознавать 10 секунд).
* `void onPageTurned(size_t estimated_word_delta)` — вызывается, когда пользователь
перелистнул страницу. Смещает текущую позицию `current_word_idx` на размер страницы и
уточняет позицию в аудио через `AnchorManager`.
* `double getAudioSyncPoint() const` — возвращает точное время для аудио, если
пользователь решил нажать Play после чтения глазами.
