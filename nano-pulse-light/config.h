#ifndef CONFIG_H
#define CONFIG_H

// Все настройки прошивки. Менять можно здесь, скетч трогать не обязательно.
//
// Питание датчика здесь 5 В (на Lolin было 3.3 В), АЦП Nano 0..1023.
// Пороги детектора скопированы с рабочего lolin-web-pulse как старт.
// Если пульс врёт или лента дёргается не в ритм — крутить в первую очередь:
//   MIN_SILA, POROG_V_SILAH, ALFA_BYSTROGO.

// --- пины ---

const int NOZHKA_DATCHIKA = A0;   // сигнал S модуля HW827
const int NOZHKA_KNOPKI = 2;      // NC-кнопка на землю, внутренний подтяг
const int NOZHKA_LENTY = 11;      // SIG платы IRF520, аппаратный PWM

// Кнопка NC между D2 и GND.
// Покой: контакты замкнуты, пин на земле, читаем LOW.
// Нажатие: контакты разомкнулись, внутренний подтяг тянет вверх, читаем HIGH.
const int UROVEN_NAZHATIYA = HIGH;

// --- датчик пульса ---

// Как часто читаем АЦП. 5 мс = 200 раз в секунду.
const unsigned long PERIOD_CHTENIYA_MS = 5;

// Сглаживание шума АЦП. При шаге 5 мс это примерно 100 мс.
const float ALFA_BYSTROGO = 0.05;

// Медленная базовая линия (~1.5 с). Вычитаем её, чтобы убрать дрейф.
const float ALFA_BAZOVOY = 0.0033;

// Оценка силы волны (~3 с). Не максимум, а средний квадрат:
// одиночный рывок пальцем меньше портит порог.
const float ALFA_SILY = 0.00167;

// Насколько сильный выброс можно взять в оценку силы.
const float PREDEL_VYBROSA = 4.0;

// Ниже этой силы считаем, что пальца нет.
// На 5 В шума меньше, чем на 3.3 В у Lolin, но старт тот же — подкрутить по факту.
const float MIN_SILA = 1.0;

// Порог удара и уровень возврата, в долях от силы волны.
const float POROG_V_SILAH = 0.90;
const float VOZVRAT_V_SILAH = 0.30;

// Живой пульс в этих границах. Ниже 40 и выше 140 считаем невозможным:
// такой удар в статистику не берём, цифру не обновляем.
const int MIN_PULS = 40;
const int MAX_PULS = 140;

// Те же границы в миллисекундах между ударами.
const unsigned long MIN_PROMEZHUTOK_MS = 60000UL / MAX_PULS;  // 140 уд/мин
const unsigned long MAX_PROMEZHUTOK_MS = 60000UL / MIN_PULS;  // 40 уд/мин
const unsigned long TAYMAUT_SIGNALA_MS = 3000;   // нет ударов — палец убрали

// Вторичный пик волны (дикротический зубец) короче настоящего удара.
const float MIN_DOLYA_OT_SREDNEGO = 0.55;
const float MAX_DOLYA_OT_SREDNEGO = 1.60;
const int SKOLKO_PROPUSKOV_DO_SBROSA = 4;
const int SKOLKO_PROMEZHUTKOV_USREDNYAT = 5;

// Скользящее окно, в котором считаем удары в минуту.
const unsigned long OKNO_PULSA_MS = 10000;

// При включении и после сна дышим с этим темпом.
// 20 ниже разрешённых 40 — так специально: медленная качка = «ещё нет живого пульса».
// Фильтр 40..140 это число не затрёт, пока не появятся настоящие удары.
const int STARTOVYY_PULS = 20;

// Запас: при 140 уд/мин за 10 с будет ~23 удара.
const int MAX_UDAROV_V_OKNE = 40;

// --- лента ---

// Режим 1 (пульс): пол и потолок яркости, проценты от максимума ШИМ.
const int MIN_YARKOST_PULSA_PROCENT = 10;
const int MAX_YARKOST_PULSA_PROCENT = 100;

// Режимы 2..6 — фиксированная яркость. Режим 7 — полностью выкл.
const int YARKOST_REZHIM_2_PROCENT = 20;
const int YARKOST_REZHIM_3_PROCENT = 40;
const int YARKOST_REZHIM_4_PROCENT = 60;
const int YARKOST_REZHIM_5_PROCENT = 80;
const int YARKOST_REZHIM_6_PROCENT = 100;

const int REZHIM_PULS = 1;
const int REZHIM_20 = 2;
const int REZHIM_40 = 3;
const int REZHIM_60 = 4;
const int REZHIM_80 = 5;
const int REZHIM_100 = 6;
const int REZHIM_VYKL = 7;
const int PERVYY_REZHIM = 1;
const int POSLEDNIY_REZHIM = 7;

// Как часто обновляем ШИМ ленты. 20 мс = 50 раз в секунду, глазу хватает.
const unsigned long PERIOD_LENTY_MS = 20;

// --- кнопка ---

const unsigned long ANTIDREBEZG_MS = 40;

// После последнего отпускания столько ждём, потом решаем: 1, 2, 3 или 4 клика.
const unsigned long PAUZA_KLIKOV_MS = 450;

// --- таймер сна, метроном и стробоскоп ---

const unsigned long TAYMER_SNA_MS = 10UL * 60UL * 1000UL;

const unsigned long METRONOM_PERIOD_MS = 1000;
const int METRONOM_MIN_PROCENT = 10;
const int METRONOM_MAX_PROCENT = 100;

// Стробоскоп: резко 0% / 100%, без плавного розжига.
// STROB_PERIOD_MS — полный цикл вспышка + пауза.
//   50  → 20 вспышек в секунду (жёстко)
//   80  → 12.5 (старт, выглядит «клубно»)
//   100 → 10
//   200 → 5 (спокойнее)
const unsigned long STROB_PERIOD_MS = 80;
const int STROB_MIN_PROCENT = 0;
const int STROB_MAX_PROCENT = 100;

// --- отладка ---

const unsigned long PERIOD_OTLADKI_MS = 1000;

#endif
