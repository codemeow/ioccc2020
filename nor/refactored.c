#include <X11/Xlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/*! \brief Ширина изображения в пикселях */
#define IMAGE_WIDTH (1220)
/*! \brief Высота изображения в пикселях */
#define IMAGE_HEIGHT (616)
/*! \brief Сдвиг верхней грани карты от левой стороны */
#define IMAGE_SHIFTX (580)

/*! \brief Количество текстур */
#define TEXTURE_COUNT (8)
/*! \brief Ширина текстуры в пикселях */
#define TEXTURE_WIDTH (64)
/*! \brief Высота текстур в пикселях */
#define TEXTURE_HEIGHT (40)
/*! \brief Ширина грани текстуры в пикселях */
#define TEXTURE_TOP_WIDTH (64)
/*! \brief Высота грани текстуры в пикселях */
#define TEXTURE_TOP_HEIGHT (32)

/*! \brief Ширина эмулируемого поля */
#define MAP_WIDTH (19)
/*! \brief Высота эмулируемого поля */
#define MAP_HEIGHT (19)
/*! \brief Количество данные в файле-чертеже. Один байт добавлен для символа '\n' */
#define MAP_FILEDATA ((MAP_WIDTH + 1) * MAP_HEIGHT)
/*! \brief Количество итераций во время расчета NOR-узлов */
#define MAP_ITERATIONS (20)

/*! \brief Содержит структуры синтаксиса файла-конфигурации */
enum map_characters {
    MAPCHAR_WIRE  = '.', /**< Провод            (ASCII = 46) */
    MAPCHAR_PLUS  = '+', /**< Провод (есть ток) (ASCII = 43) */
    MAPCHAR_MINUS = '-', /**< Провод (нет тока) (ASCII = 45) */
    MAPCHAR_NOR   = '>', /**< NOR-элемент       (ASCII = 62) */
    MAPCHAR_EMPTY = ' ', /**< Пустой блок       (ASCII = 32) */
};

/*! \brief Содержит индексы текстур */
enum textures_indexes {
    TEXINDEX_EMPTY = (0), /**< Индекс пустой текстуры                 */
    TEXINDEX_MINUS = (1), /**< Индекс текстуры "выключенного провода" */
    TEXINDEX_WIRE  = (2), /**< Индекс текстуры нейтрального провода   */
    TEXINDEX_PLUS  = (3), /**< Индекс текстуры "включенного" провода  */
    /**/
    TEXINDEX_NOR   = (6), /**< Индекс текстуры NOR-элемента           */
    TEXINDEX_HOLE  = (7)  /**< Индекс текстуры отверстия на плате     */
};

/*! \brief Список инструкций векторного интерпретатора */
enum textures_instructions {
    TEXVEC_LINESE = (0), /**< Линия SE                          */
    TEXVEC_LINESW = (1), /**< Линия SW                          */
    TEXVEC_LINEDW = (2), /**< Линия вниз                        */
    TEXVEC_FILL   = (3), /**< Заливка                           */
    TEXVEC_EXIT   = (4), /**< Конец списка инструкций           */
    TEXVEC_REPEAT = (5), /**< Повтор инструкций другой текстуры */
    TEXVEC_COLOR  = (6)  /**< Выбор цвета                       */
};

/*! \brief Аргументы программы */
enum program_arguments { 
    ARG_PROGRAM,  /**< Имя самой программы */
    ARG_BLUEPRINT /**< Имя файла-чертежа   */
};

/*! \brief Бинарные данные пикселей изображения.
 * Типизированы к int для оперирования пикселями вместо каналов */
int pixdata[IMAGE_HEIGHT][IMAGE_WIDTH];
/*! \brief Текстуры блоков, отображаемых на поле */
int textures[TEXTURE_COUNT][TEXTURE_HEIGHT][TEXTURE_WIDTH] = { 0 };
/*! \brief Данные эмулируемого поля. 
 * Один байт добавлен для упрощения обработки символа '\n' */
char mapdata[MAP_HEIGHT][MAP_WIDTH + 1];
/*! \brief Зерно генератора случайных чисел */
int random = 2166136261;

/*! \brief Экземпляр дисплея Xlib */
Display * display;
/*! \brief Экземпляр главного окна Xlib */
Window window;
/*! \brief Изображение для вывода на экран */
XImage * image;

/*! \brief Палитра цветов для векторной отрисовки 
 * \note Черный имеет цвет {001} для отличия от "прозрачного" черного {000} */
static const int texturepalette[] = { 
                         /*  Светлее   ▁▁▂▂▃▃▄▄▅▅▆▆▇▇██   Темнее */    
    /* Светло-коричневый */ 0xe6ac73, 0xcd9966, 0xb48659, 0x9b734c,
    /* Темно-коричневый  */ 0x806040, 0x674d33, 0x4e3a26, 0x352719,
    /* Зеленый           */ 0x59b368, 0x40a05b, 0x278d4e, 0x0e7a41,
    /* Серый             */ 0xc6c6c6, 0xadb3b9, 0x94a0ac, 0x7b8d9f,
    /* Черный            */ 0x000001
};

/*! \brief Векторные инструкции для рисования текстур */
static const char * vecdata[TEXTURE_COUNT] = { 
    /* TEXINDEX_EMPTY */
    (char[]) { TEXVEC_EXIT }, 
    /* TEXINDEX_MINUS */
    (char[]) { TEXVEC_COLOR,   5,            TEXVEC_LINESE, 32,  5, 16, 
               TEXVEC_LINESE,  0, 21, 16,    TEXVEC_LINESW, 63, 21, 16, 
               TEXVEC_LINEDW,  0, 21,  4,    TEXVEC_LINEDW, 31, 36,  4, 
               TEXVEC_LINESW, 31,  5, 16,    TEXVEC_LINESE,  0, 24, 16, 
               TEXVEC_LINESW, 63, 24, 16,    TEXVEC_LINEDW, 63, 21,  4, 
               TEXVEC_LINEDW, 32, 36,  4,    TEXVEC_COLOR,   4, 
               TEXVEC_FILL,   31, 8,         TEXVEC_COLOR,   6,
               TEXVEC_FILL,   33, 38,        TEXVEC_COLOR,   7, 
               TEXVEC_FILL,   30, 38,        TEXVEC_EXIT },
    /* TEXINDEX_WIRE */
    (char[]) { TEXVEC_REPEAT,  1 }, 
    /* TEXINDEX_PLUS */
    (char[]) { TEXVEC_COLOR,   1,            TEXVEC_LINESE, 32,  5, 16,
               TEXVEC_LINESE,  0, 21, 16,    TEXVEC_LINESW, 63, 21, 16, 
               TEXVEC_LINEDW, 63, 21,  4,    TEXVEC_LINEDW, 31, 36,  4, 
               TEXVEC_LINESW, 31,  5, 16,    TEXVEC_LINESE,  0, 24, 16, 
               TEXVEC_LINESW, 63, 24, 16,    TEXVEC_LINEDW, 63, 21,  4, 
               TEXVEC_LINEDW, 32, 36,  4,    TEXVEC_COLOR,   0, 
               TEXVEC_FILL,   31,  8,        TEXVEC_COLOR,   2,
               TEXVEC_FILL,   33, 38,        TEXVEC_COLOR,   3, 
               TEXVEC_FILL,   30, 38,        TEXVEC_EXIT },
    /* Не используется */
    (char[]) { TEXVEC_EXIT }, 
    /* Не используется */
    (char[]) { TEXVEC_EXIT },
    /* TEXINDEX_NOR */
    (char[]) { TEXVEC_COLOR,   9,            TEXVEC_LINESE, 32,  2, 16, 
               TEXVEC_LINESE,  0, 18, 16,    TEXVEC_LINESW, 63, 18, 16, 
               TEXVEC_LINEDW,  0, 18,  4,    TEXVEC_LINEDW, 31, 33,  4, 
               TEXVEC_LINESW, 31,  2, 16,    TEXVEC_LINESE,  0, 21, 16, 
               TEXVEC_LINESW, 63, 21, 16,    TEXVEC_LINEDW, 63, 18,  4, 
               TEXVEC_LINEDW, 32, 33,  4,    TEXVEC_COLOR,   8,
               TEXVEC_FILL,   31,  5,        TEXVEC_COLOR,  10,  
               TEXVEC_FILL,   33, 35,        TEXVEC_COLOR,  11,
               TEXVEC_FILL,   30, 35,        TEXVEC_EXIT }, 
    /* TEXINDEX_HOLE */
    (char[]) { TEXVEC_COLOR,  13,            TEXVEC_LINESE, 32, 9, 12, 
               TEXVEC_LINESE,  8, 21, 12,    TEXVEC_LINESW, 31, 9, 12, 
               TEXVEC_LINESW, 55, 21, 12,    TEXVEC_LINESE, 32, 12, 9, 
               TEXVEC_LINESE, 14, 21,  9,    TEXVEC_LINESW, 31, 12, 9, 
               TEXVEC_LINESW, 49, 21,  9,    TEXVEC_COLOR,  14, 
               TEXVEC_LINEDW, 31, 13,  4,    TEXVEC_LINESW, 31, 16, 7, 
               TEXVEC_FILL,   30, 14,        TEXVEC_COLOR,  12, 
               TEXVEC_FILL,   32, 11,        TEXVEC_COLOR,  15, 
               TEXVEC_LINEDW, 32, 13,  4,    TEXVEC_LINESE, 32, 16, 7, 
               TEXVEC_FILL,   33, 14,        TEXVEC_COLOR,  16, 
               TEXVEC_FILL,   31, 19,        TEXVEC_EXIT }
};

static void _texture_draw(int t, int x, int y);

/*! \brief Создает изображение и сопутствующие сущности */
static void _image_create(void) {
    display = XOpenDisplay(0);
    window = XCreateSimpleWindow(display, 
                                 RootWindow(display, DefaultScreen(display)), 
                                 0, 0, IMAGE_WIDTH, IMAGE_HEIGHT, 1, 0, 0);
    image = XCreateImage(display, 
                     DefaultVisual(display, DefaultScreen(display)), 
                     DefaultDepth(display, DefaultScreen(display)),
                     2, 0, (char *) pixdata, IMAGE_WIDTH, IMAGE_HEIGHT, 32, 0);
}

/* \brief Обнуляет изображение, заполняя его черным цветом */
static void _image_reset(void) {
    for (int y = 0; y < IMAGE_HEIGHT; y++)
        for (int x = 0; x < IMAGE_WIDTH; x++)
            pixdata[y][x] = 0;
}

/*! \brief Конвертирует символ из чертежа в индекс текстуры
 * \param[in] elem Символ чертежа
 * \return Индекс текстуры */
static int _map2texture(char elem) {
    return ((elem >> 4) & 1) << 2 | (elem & 3);
}

/*! \brief Собирает изображение из отдельных тайлов согласно карте */
static void _image_compile(void) {
    for (int y = 0; y < MAP_HEIGHT; y++)
        for (int x = 0; x < MAP_WIDTH; x++)
            _texture_draw(_map2texture(mapdata[y][x]), x, y);
}

/*! \brief Рисует изображение на экране */
static void _image_draw(void) {
    XPutImage(display, window, 
              DefaultGC(display, DefaultScreen(display)), 
              image, 0, 0, 0, 0, IMAGE_WIDTH, IMAGE_HEIGHT);
    XMapWindow(display, window);
    XFlush(display);
}

/*! \brief Рисует отверстия на печатной плате */
static void _image_drill(void) {
    for (int y = 0; y < MAP_HEIGHT; y++)
            for (int x = 0; x < MAP_WIDTH; x++)
                if ((x % 14 == 2) && (y % 4 == 3))
                    _texture_draw(TEXINDEX_HOLE, x, y);
}

/*! \brief Применяет шейдер LCD-эффекта на изображение
 * \param[in] x X-координата изображения
 * \param[in] y Y-координата изображения */
static void _shader_lcd(int x, int y) {
    pixdata[y][x] &= 0xe0e0e0 | (31 << ((x % 3) << 3));
}

/*! \brief Применяет шейдер случайного шума на изображение
 * \param[in] x X-координата изображения
 * \param[in] y Y-координата изображения */
static void _shader_noise(int x, int y) {
    pixdata[y][x] += 0x0f0f0f & (random *= 16777619);
}

/*! \brief Накладывает на изображение различные эффекты */
static void _image_postprocess(void) {
    for (int y = 0; y < IMAGE_HEIGHT; y++)
        for (int x = 0; x < IMAGE_WIDTH; x++) {
            _shader_lcd(x, y);
            _shader_noise(x, y);
        }
}

/*! \brief Отрисовывает текстуру на главном холсте по указанным координатам
 * \param[in] t Индекс текстуры
 * \param[in] x X координата тайла
 * \param[in] y Y координата тайла */
static void _texture_draw(int t, int x, int y) {
    for (int ty = 0; ty < TEXTURE_HEIGHT; ty++)
        for (int tx = 0; tx < TEXTURE_WIDTH; tx++)
            if (textures[t][ty][tx])
                pixdata[ty + 
                        y * (TEXTURE_TOP_HEIGHT / 2) + 
                        x * (TEXTURE_TOP_HEIGHT / 2)]
                       [tx + 
                        IMAGE_SHIFTX + 
                        x * (TEXTURE_TOP_WIDTH / 2) - 
                        y * TEXTURE_TOP_HEIGHT] = textures[t][ty][tx];
}

/*! \brief Производит закраску области текстуры
 * \param[in] t Индекс текстуры
 * \param[in] x X-координата начала
 * \param[in] y Y-координата начала
 * \param[in] source Исходный цвет
 * \param[in] target Новый цвет */
static void _texture_fill(int t, int x, int y, int source, int target) {
    if ((x >= TEXTURE_WIDTH || y >= TEXTURE_HEIGHT || 
         x < 0 || y < 0) || 
         (textures[t][y][x] == target) || 
         (textures[t][y][x] != source))
        return;
    textures[t][y][x] = target;
    _texture_fill(t, x - 1, y, source, target);
    _texture_fill(t, x + 1, y, source, target);
    _texture_fill(t, x, y - 1, source, target);
    _texture_fill(t, x, y + 1, source, target);
}

/*! \brief Рисует изометрическую линию по направлению SE
 * \param[in] t Индекс текстуры
 * \param[in] x X-координата начала
 * \param[in] y Y-координата начала
 * \param[in] c Длина линии по короткой стороне
 * \param[in] color Цвет рисования */
static void _texture_linese(int t, int x, int y, int c, int color) {
    while (c--) {
        textures[t][y][x++] = color;
        textures[t][y++][x++] = color;
    }
}

/*! \brief Рисует изометрическую линию по направлению SW
 * \param[in] t Индекс текстуры
 * \param[in] x X-координата начала
 * \param[in] y Y-координата начала
 * \param[in] c Длина линии по короткой стороне
 * \param[in] color Цвет рисования */
static void _texture_linesw(int t, int x, int y, int c, int color) {
    while (c--) {
        textures[t][y][x--] = color;
        textures[t][y++][x--] = color;
    }
}

/*! \brief Рисует линию вниз
 * \param[in] t Индекс текстуры
 * \param[in] x X-координата начала
 * \param[in] y Y-координата начала
 * \param[in] c Длина линии
 * \param[in] color Цвет рисования */
static void _texture_linedown(int t, int x, int y, int c, int color) {
    while (c--)
        textures[t][y++][x] = color;
}

/*! \brief Создает все текстуры, интерпретатор векторных инструкций */
static void _textures_create(void) {
    for (int tex = 0; tex < TEXTURE_COUNT; tex++) {
        const char * ptr = vecdata[tex];
        int c = 0;
        while (*ptr != TEXVEC_EXIT) {
            switch (*ptr) {
            case TEXVEC_LINESE:
                _texture_linese(tex, ptr[1], ptr[2], ptr[3], c);
                ptr += 4;
                break;
                
            case TEXVEC_LINESW:
                _texture_linesw(tex, ptr[1], ptr[2], ptr[3], c);
                ptr += 4;
                break;
                
            case TEXVEC_LINEDW:
                _texture_linedown(tex, ptr[1], ptr[2], ptr[3], c);
                ptr += 4;
                break;
                
            case TEXVEC_FILL:
                _texture_fill(tex, ptr[1], ptr[2], 0, c);
                ptr += 3;
                break;
                
            case TEXVEC_REPEAT:
                ptr = vecdata[ptr[1]];
                break;
                
            case TEXVEC_COLOR:
                c = texturepalette[ptr[1]];
                ptr += 2;
                break;
            }
        }
    }
}

/*! \brief Читает данные файла-чертежа и загружает их в карту 
 * \param[in] filename Имя файла-чертежа */
static void _map_read(const char * filename) {
    int f = open(filename, 0);
    read(f, mapdata, MAP_FILEDATA);
    close(f);
}

/*! \brief Заменяет иллюстративные входы из файла-конфигурации на вход
 * в виде провода чтобы работала логика распространения фронта волны */
static void _map_wire_inputs(void) {
    for (int y = 0; y < MAP_HEIGHT; y++)
        for (int x = 0; x < MAP_WIDTH; x++)
            if ((x % 14 == 2) && (y % 4 == 3))
                mapdata[y][x] = MAPCHAR_WIRE;
}

/*! \brief Производит заливку проводника нужным состоянием
 * \param[in] t Игнорируется, артефакт автогенерации кода 
 * \param[in] x X-координата заливки
 * \param[in] y Y-координата заливки
 * \param[in] c Исходное состояние
 * \param[in] l Целевое состояние */
static void _map_fill(int t, int x, int y, int c, int l) {
    if ((x >= MAP_WIDTH || y >= MAP_HEIGHT || 
         x < 0 || y < 0) || (mapdata[y][x] == l)
        || (mapdata[y][x] != c))
        return;
    mapdata[y][x] = l;
    _map_fill(t, x - 1, y, c, l);
    _map_fill(t, x + 1, y, c, l);
    _map_fill(t, x, y - 1, c, l);
    _map_fill(t, x, y + 1, c, l);
}

/*! \brief Включает соответствующие входы схемы в зависимости от значения
 * счетчика.
 * \param[in] counter Счетчик */
static void _map_wire_counter(int counter) {
    _map_fill(0, 2, 3,  mapdata[3][2],  counter & 1 ? MAPCHAR_PLUS : MAPCHAR_MINUS);
    _map_fill(0, 2, 7,  mapdata[7][2],  counter & 2 ? MAPCHAR_PLUS : MAPCHAR_MINUS);
    _map_fill(0, 2, 11, mapdata[11][2], counter & 4 ? MAPCHAR_PLUS : MAPCHAR_MINUS);
    _map_fill(0, 2, 15, mapdata[15][2], counter & 8 ? MAPCHAR_PLUS : MAPCHAR_MINUS);
}

/*! \brief Проводит расчет выходного (результирующего) тока после NOR-узла */
static void _map_process_gates(void) {       
    for (int i = 0; i < MAP_ITERATIONS; i++)
        for (int y = 0; y < MAP_HEIGHT; y++)
            for (int x = 0; x < MAP_WIDTH; x++)
                if (mapdata[y][x] == MAPCHAR_NOR)
                    _map_fill(0, x + 1, y, mapdata[y][x + 1],
                        !(mapdata[y - 1][x] == MAPCHAR_PLUS ? 1 : 0
                       || mapdata[y + 1][x] == MAPCHAR_PLUS ? 1 : 0) ? 
                            MAPCHAR_PLUS : MAPCHAR_MINUS);
}

int main(int argc, char * args[]) {
    _image_create();
    _textures_create();
    
    unsigned int counter = 1;
    while (counter++) {
        _map_read(args[ARG_BLUEPRINT]);
        _map_wire_inputs();
        _map_wire_counter(counter);
        _map_process_gates();
        
        _image_reset();          
        _image_compile();
        _image_drill();
        _image_postprocess();        
        _image_draw();
        
        sleep(1);
    } 
    return 0;
}
