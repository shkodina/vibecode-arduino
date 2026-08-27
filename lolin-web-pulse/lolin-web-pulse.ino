// Пульсометр на Lolin NodeMCU v3 (ESP8266) с веб-страничкой.
//
// Что делает скетч:
//   1. Читает аналоговый датчик пульса с ножки A0.
//   2. Ищет в сигнале удары сердца и считает пульс (уд/мин).
//   3. Поднимает веб-сервер и отдаёт страничку.
//   4. Страничка раз в 300 мс спрашивает свежие данные и пульсирует
//      красным в темп сердца. Пульс виден в левом верхнем углу.
//
// Подключение датчика (3 провода):
//   VCC (красный)  -> 3V3
//   GND (чёрный)   -> GND
//   Signal         -> A0
//
// ВАЖНО: у ESP8266 только один аналоговый вход, это A0. На цифровые ножки
// (D1, D8 и прочие) такой датчик подключать нельзя - они видят только 0 и 1,
// а нам нужна плавная волна.
//
// Про параметры алгоритма. Они подобраны под реально измеренный сигнал
// этого датчика, а не взяты с потолка:
//   - размах полезного сигнала около 25-30 единиц АЦП;
//   - шум одного замера примерно 1-2 единицы;
//   - медленный дрейф уровня (дыхание, прижим пальца) - десятки единиц.
// Отсюда сглаживание, отдельная базовая линия и медленное затухание оценки
// амплитуды. Если поставить их агрессивнее, порог начнёт болтаться в шуме
// и пульс будет прыгать.

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// --- настройки wifi ---
//
// Имя сети и пароль в коде НЕ хранятся, они приходят снаружи на этапе сборки
// через определения компилятора WIFI_SSID и WIFI_PASS.
// Как их передавать - смотри readme.md, раздел "Пароль от wifi".
//
// Если при сборке их не передали - сборка падает с ошибкой. Это сделано
// специально: arduino-cli по умолчанию скрывает предупреждения компилятора,
// поэтому обычный #warning остался бы незамеченным, и мы бы залили в плату
// прошивку с пустым паролем, а выяснили это только по молчащему модулю.

#ifndef WIFI_SSID
#error "WIFI_SSID ne zadan. Sobiray cherez ./sborka.sh, smotri readme.md"
#endif

#ifndef WIFI_PASS
#error "WIFI_PASS ne zadan. Sobiray cherez ./sborka.sh, smotri readme.md"
#endif

const char* WIFI_IMYA = WIFI_SSID;
const char* WIFI_PAROL = WIFI_PASS;

// --- настройки датчика ---
const int NOZHKA_DATCHIKA = A0;

// Как часто читаем датчик. 5 мс это 200 раз в секунду.
const unsigned long PERIOD_CHTENIYA_MS = 5;

// --- сглаживание сигнала ---
// Сглаживание убирает шум АЦП, но оставляет форму удара.
// Коэффициент 0.05 при шаге 5 мс даёт постоянную времени около 100 мс.
// Пульс это примерно 1-1.5 Гц, а шум широкополосный, поэтому такое
// сглаживание давит шум примерно в 1.6 раза, а волну почти не портит.
// Ставили 40 мс - сигнал оказался слишком близко к шуму, и детектор
// начинал ловить шум и упираться в защитный минимум 350 мс (это ~170 уд/мин).
const float ALFA_BYSTROGO = 0.05;

// Базовая линия - это медленное среднее, то есть уровень без пульсаций.
// Коэффициент 0.0033 даёт постоянную времени около 1.5 секунды.
// Вычитая её из сигнала, получаем чистую пульсовую волну вокруг нуля.
const float ALFA_BAZOVOY = 0.0033;

// --- оценка силы пульсовой волны ---
// Силу волны считаем через среднеквадратичное значение, а НЕ через максимум.
// Причина: прижим и смещение пальца дают выбросы в 10 раз сильнее пульса
// (замеряли 200-270 против настоящих 28). Максимум такой выброс запоминает
// и надолго задирает порог, из-за чего настоящие удары пропускаются.
// Среднеквадратичное значение к одиночным выбросам куда устойчивее.
// Коэффициент 0.00167 при шаге 5 мс даёт постоянную времени около 3 секунд.
const float ALFA_SILY = 0.00167;

// Дополнительная защита от выбросов: при подсчёте силы ограничиваем сигнал
// вот таким числом текущих сил. Один рывок пальцем больше не портит оценку.
const float PREDEL_VYBROSA = 4.0;

// Если сила волны меньше этого - считаем, что пальца нет.
// Замеряли: палец лежит спокойно - сила 2.3-3.0 (пики волны до 7),
// пальца нет - меньше 0.5. Порог 1.0 держится посередине.
// Раньше стояло 2.0, и это было вплотную к рабочему уровню:
// сила чуть колебалась и модуль постоянно терял палец.
const float MIN_SILA = 1.0;

// Порог срабатывания, в долях от силы волны.
// Для ровной волны пик примерно в 1.4 раза больше силы, поэтому 0.9 -
// это чуть выше середины подъёма.
const float POROG_V_SILAH = 0.90;

// Чтобы поймать следующий удар, волна должна сначала опуститься вот досюда.
// Разница с порогом - защита от дребезга.
const float VOZVRAT_V_SILAH = 0.30;

// --- проверка промежутков между ударами ---
// Минимальный промежуток: 350 мс это ограничение сверху в 171 уд/мин.
const unsigned long MIN_PROMEZHUTOK_MS = 350;

// Максимальный промежуток: 2000 мс это 30 уд/мин.
const unsigned long MAX_PROMEZHUTOK_MS = 2000;

// Если удров не было столько времени - обнуляем показания.
const unsigned long TAYMAUT_SIGNALA_MS = 3000;

// Сколько последних промежутков усредняем.
const int SKOLKO_PROMEZHUTKOV_USREDNYAT = 5;

// Промежуток короче этой доли от среднего считаем ложным срабатыванием.
// Так отсекается дикротический зубец - вторичный пик пульсовой волны,
// из-за которого пульс показывался вдвое больше настоящего.
const float MIN_DOLYA_OT_SREDNEGO = 0.55;

// Промежуток длиннее этой доли от среднего означает, что один удар мы
// пропустили. Такой промежуток в усреднение брать нельзя: он двойной и
// тянет цифру вниз (замеряли 83 вместо 100). Удар при этом считаем -
// иначе анимация на странице рассинхронизируется.
const float MAX_DOLYA_OT_SREDNEGO = 1.60;

// Если столько промежутков подряд оказались двойными - значит пульс
// реально изменился или контакт с датчиком другой. Тогда набираем
// статистику заново, а не отбрасываем всё бесконечно.
const int SKOLKO_PROPUSKOV_DO_SBROSA = 4;

// --- переменные обработки сигнала ---
float bystroe_srednee = 0;    // сглаженный сигнал
float bazovaya_liniya = 0;    // медленный уровень, вокруг которого пульсации
bool pervoe_chtenie = true;   // при первом чтении надо задать начальные значения

// Средний квадрат волны. Корень из него - это сила волны.
float sredniy_kvadrat = 0;

bool byli_nizhe_poroga = true;

unsigned long vremya_poslednego_udara = 0;
unsigned long promezhutki[SKOLKO_PROMEZHUTKOV_USREDNYAT];
int skolko_promezhutkov_nabrali = 0;
int kuda_pisat_promezhutok = 0;

// Что делать с найденным промежутком между ударами.
const int RESHENIE_NE_UDAR = 0;    // это не удар, забыть совсем
const int RESHENIE_ZAPOMNIT = 1;   // обычный удар, взять в статистику
const int RESHENIE_PROPUSK = 2;    // удар был, но один мы пропустили

// Сколько двойных промежутков попалось подряд.
int propuskov_podryad = 0;

// Счётчик удров, по нему страничка ловит новый удар.
unsigned long schetchik_udarov = 0;

// Посчитанный пульс. 0 значит "сигнала нет".
int pulse_ud_v_minutu = 0;

unsigned long vremya_poslednego_chteniya = 0;

// --- диагностика: запись сигнала для отладки ---
// Пишем уже очищенный сигнал (без базовой линии), усреднённый по 100 мс.
const int RAZMER_BUFERA = 400;
const int SKOLKO_USREDNYAT_DLYA_ZAPISI = 20;

long summa_dlya_zapisi = 0;
int skolko_slozhili = 0;
int bufer_zapisi[RAZMER_BUFERA];
int kuda_pisat_v_bufer = 0;
bool bufer_polon = false;

ESP8266WebServer server(80);

// Страничка. Вся анимация живёт в браузере, модуль отдаёт только числа.
const char* STRANICA_HTML = R"HTML(
<!DOCTYPE html>
<html lang="ru">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Пульс</title>
  <style>
    html, body {
      margin: 0;
      padding: 0;
      height: 100%;
      overflow: hidden;
      background: #000;
    }
    #ugolok {
      position: fixed;
      top: 16px;
      left: 20px;
      font-family: sans-serif;
      color: #fff;
      text-shadow: 0 0 6px #000;
    }
    #cifra {
      font-size: 64px;
      font-weight: bold;
      line-height: 1;
    }
    #podpis {
      font-size: 16px;
      opacity: 0.8;
      margin-top: 4px;
    }
  </style>
</head>
<body>
  <div id="ugolok">
    <div id="cifra">--</div>
    <div id="podpis">уд/мин</div>
  </div>

<script>
var pulse = 0;
var vremyaPoslednegoUdara = 0;
var nomerPoslednegoUdara = -1;

// Спрашиваем у модуля свежие данные.
function sprositDannye() {
  fetch('/dannye')
    .then(function (otvet) { return otvet.json(); })
    .then(function (dannye) {
      pulse = dannye.pulse;

      // Если номер удара изменился - подстраиваем свои часы под модуль.
      if (dannye.nomer !== nomerPoslednegoUdara) {
        nomerPoslednegoUdara = dannye.nomer;
        vremyaPoslednegoUdara = Date.now() - dannye.davno;
      }

      document.getElementById('cifra').textContent = pulse > 0 ? pulse : '--';
      document.getElementById('podpis').textContent =
        pulse > 0 ? 'уд/мин' : 'приложите палец';
    })
    .catch(function () {
      document.getElementById('cifra').textContent = '--';
    });
}

// Рисуем кадр: считаем яркость красного и красим фон.
function narisovatKadr() {
  var yarkost = 0;

  if (pulse > 0) {
    var dlinaUdara = 60000 / pulse;
    var faza = ((Date.now() - vremyaPoslednegoUdara) % dlinaUdara) / dlinaUdara;

    // Яркий на пике (фаза около нуля) и тусклый на спаде.
    yarkost = (1 - faza) * (1 - faza);
  }

  var krasnyy = Math.round(25 + yarkost * 230);
  document.body.style.background = 'rgb(' + krasnyy + ', 0, 0)';

  requestAnimationFrame(narisovatKadr);
}

setInterval(sprositDannye, 300);
sprositDannye();
requestAnimationFrame(narisovatKadr);
</script>
</body>
</html>
)HTML";

void otdatGlavnuyuStranicu() {
  server.send(200, "text/html", STRANICA_HTML);
}

// Отдаём свежие данные в виде json.
// pulse     - удары в минуту, 0 если сигнала нет
// nomer     - номер последнего удара
// davno     - сколько миллисекунд прошло с последнего удара
// sila      - сила пульсовой волны, для отладки
void otdatDannye() {
  unsigned long skolko_proshlo = millis() - vremya_poslednego_udara;
  float sila = sqrt(sredniy_kvadrat);

  String otvet = "{";
  otvet += "\"pulse\":" + String(pulse_ud_v_minutu) + ",";
  otvet += "\"nomer\":" + String(schetchik_udarov) + ",";
  otvet += "\"davno\":" + String(skolko_proshlo) + ",";
  otvet += "\"sila\":" + String(sila, 1);
  otvet += "}";

  server.send(200, "application/json", otvet);
}

// Диагностика: отдаём запись очищенного сигнала.
// Значения умножены на 100, шаг по времени - 100 мс.
void otdatZapisSignala() {
  String otvet = "shag_ms=";
  otvet += String(PERIOD_CHTENIYA_MS * SKOLKO_USREDNYAT_DLYA_ZAPISI);
  otvet += " mnozhitel=100";
  otvet += " sila=" + String(sqrt(sredniy_kvadrat), 1) + "\n";

  int skolko_otdavat = bufer_polon ? RAZMER_BUFERA : kuda_pisat_v_bufer;

  for (int i = 0; i < skolko_otdavat; i++) {
    int nomer = bufer_polon ? (kuda_pisat_v_bufer + i) % RAZMER_BUFERA : i;
    otvet += String(bufer_zapisi[nomer]);
    otvet += "\n";
  }

  server.send(200, "text/plain", otvet);
}

void otdatNeNajdeno() {
  server.send(404, "text/plain", "Takoy stranicy net");
}

// Считаем средний промежуток между ударами.
unsigned long sredniy_promezhutok() {
  if (skolko_promezhutkov_nabrali == 0) {
    return 0;
  }

  unsigned long summa = 0;
  for (int i = 0; i < skolko_promezhutkov_nabrali; i++) {
    summa += promezhutki[i];
  }
  return summa / skolko_promezhutkov_nabrali;
}

// Пересчитываем пульс по накопленным промежуткам.
void poschitatPulse() {
  unsigned long sredniy = sredniy_promezhutok();

  if (sredniy == 0) {
    pulse_ud_v_minutu = 0;
    return;
  }

  // В минуте 60000 миллисекунд.
  pulse_ud_v_minutu = 60000 / sredniy;
}

void zapomnitPromezhutok(unsigned long promezhutok) {
  promezhutki[kuda_pisat_promezhutok] = promezhutok;

  kuda_pisat_promezhutok++;
  if (kuda_pisat_promezhutok >= SKOLKO_PROMEZHUTKOV_USREDNYAT) {
    kuda_pisat_promezhutok = 0;
  }

  if (skolko_promezhutkov_nabrali < SKOLKO_PROMEZHUTKOV_USREDNYAT) {
    skolko_promezhutkov_nabrali++;
  }
}

void sbrositPokazaniya() {
  skolko_promezhutkov_nabrali = 0;
  kuda_pisat_promezhutok = 0;
  pulse_ud_v_minutu = 0;
  propuskov_podryad = 0;
}

// Записываем очищенный сигнал в буфер для отладки.
void zapisatVBufer(float ochishennyy_signal) {
  summa_dlya_zapisi += (long)(ochishennyy_signal * 100);
  skolko_slozhili++;

  if (skolko_slozhili < SKOLKO_USREDNYAT_DLYA_ZAPISI) {
    return;
  }

  bufer_zapisi[kuda_pisat_v_bufer] =
      (int)(summa_dlya_zapisi / SKOLKO_USREDNYAT_DLYA_ZAPISI);

  kuda_pisat_v_bufer++;
  if (kuda_pisat_v_bufer >= RAZMER_BUFERA) {
    kuda_pisat_v_bufer = 0;
    bufer_polon = true;
  }

  summa_dlya_zapisi = 0;
  skolko_slozhili = 0;
}

// Решаем, что делать с промежутком.
int reshenie_po_promezhutku(unsigned long promezhutok) {
  // Совсем короткие промежутки - это точно не удары.
  if (promezhutok < MIN_PROMEZHUTOK_MS) {
    return RESHENIE_NE_UDAR;
  }

  // Пока статистики мало, доверяем всему, что прошло по времени.
  if (skolko_promezhutkov_nabrali < 3) {
    return RESHENIE_ZAPOMNIT;
  }

  unsigned long sredniy = sredniy_promezhutok();

  // Сильно короче среднего - это вторичный пик волны, а не новый удар.
  // Иначе пульс покажется вдвое больше настоящего.
  if (promezhutok < sredniy * MIN_DOLYA_OT_SREDNEGO) {
    return RESHENIE_NE_UDAR;
  }

  // Сильно длиннее среднего - значит один удар мы пропустили.
  // Промежуток двойной, в статистику его брать нельзя.
  if (promezhutok > sredniy * MAX_DOLYA_OT_SREDNEGO) {
    return RESHENIE_PROPUSK;
  }

  return RESHENIE_ZAPOMNIT;
}

// Главная работа: читаем датчик и ищем удары.
void obrabotatDatchik() {
  unsigned long seychas = millis();

  if (seychas - vremya_poslednego_chteniya < PERIOD_CHTENIYA_MS) {
    return;
  }
  vremya_poslednego_chteniya = seychas;

  int syroy_signal = analogRead(NOZHKA_DATCHIKA);

  // При самом первом чтении задаём начальные значения, чтобы сглаживание
  // не подтягивалось к реальному уровню с нуля несколько секунд.
  if (pervoe_chtenie) {
    pervoe_chtenie = false;
    bystroe_srednee = syroy_signal;
    bazovaya_liniya = syroy_signal;
    return;
  }

  // Шаг 1. Сглаживаем сигнал, чтобы убрать шум АЦП.
  bystroe_srednee += (syroy_signal - bystroe_srednee) * ALFA_BYSTROGO;

  // Шаг 2. Считаем базовую линию - медленный уровень сигнала.
  bazovaya_liniya += (bystroe_srednee - bazovaya_liniya) * ALFA_BAZOVOY;

  // Шаг 3. Вычитаем базовую линию. Получаем пульсовую волну вокруг нуля.
  // Так медленный дрейф от дыхания и прижима пальца больше не мешает.
  float volna = bystroe_srednee - bazovaya_liniya;

  zapisatVBufer(volna);

  // Шаг 4. Обновляем оценку силы волны.
  float sila = sqrt(sredniy_kvadrat);

  // Перед подсчётом ограничиваем выброс, чтобы рывок пальцем
  // не задрал оценку силы на несколько секунд вперёд.
  float volna_dlya_sily = volna;
  float predel = sila * PREDEL_VYBROSA;
  if (sila > 0 && volna_dlya_sily > predel) {
    volna_dlya_sily = predel;
  }
  if (sila > 0 && volna_dlya_sily < -predel) {
    volna_dlya_sily = -predel;
  }

  sredniy_kvadrat += (volna_dlya_sily * volna_dlya_sily - sredniy_kvadrat) * ALFA_SILY;
  sila = sqrt(sredniy_kvadrat);

  // Шаг 5. Если сила волны слишком мала - пальца на датчике нет.
  if (sila < MIN_SILA) {
    if (pulse_ud_v_minutu != 0) {
      sbrositPokazaniya();
    }
    byli_nizhe_poroga = true;
    return;
  }

  // Порог и уровень возврата считаем от силы волны.
  // Волна колеблется вокруг нуля, поэтому от нуля и отмеряем.
  float porog = sila * POROG_V_SILAH;
  float uroven_vozvrata = sila * VOZVRAT_V_SILAH;

  // Шаг 6. Удар - это переход волны снизу вверх через порог.
  if (byli_nizhe_poroga && volna > porog) {
    byli_nizhe_poroga = false;

    unsigned long promezhutok = seychas - vremya_poslednego_udara;
    int reshenie = reshenie_po_promezhutku(promezhutok);

    if (reshenie != RESHENIE_NE_UDAR) {
      // Слишком долгая пауза означает, что непрерывность потерялась.
      if (promezhutok > MAX_PROMEZHUTOK_MS || vremya_poslednego_udara == 0) {
        sbrositPokazaniya();

      } else if (reshenie == RESHENIE_PROPUSK) {
        // Промежуток двойной: удар считаем, но в статистику не берём.
        propuskov_podryad++;

        // Если такое повторяется - пульс реально изменился, учимся заново.
        if (propuskov_podryad >= SKOLKO_PROPUSKOV_DO_SBROSA) {
          sbrositPokazaniya();
        }

      } else {
        propuskov_podryad = 0;
        zapomnitPromezhutok(promezhutok);
        poschitatPulse();
      }

      vremya_poslednego_udara = seychas;
      schetchik_udarov++;
    }
  }

  // Шаг 7. Ждём, пока волна опустится, чтобы ловить следующий удар.
  if (!byli_nizhe_poroga && volna < uroven_vozvrata) {
    byli_nizhe_poroga = true;
  }

  // Шаг 8. Давно не было удров - значит палец убрали.
  if (vremya_poslednego_udara > 0 &&
      seychas - vremya_poslednego_udara > TAYMAUT_SIGNALA_MS) {
    sbrositPokazaniya();
  }
}

void podklyuchitsyaKWifi() {
  Serial.println();
  Serial.print("Podklyuchayus k wifi: ");
  Serial.println(WIFI_IMYA);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_IMYA, WIFI_PAROL);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Wifi podklyuchen.");
  Serial.print("IP-adres modulya: ");
  Serial.println(WiFi.localIP());
  Serial.println("Otkroy etot adres v brauzere.");
}

void setup() {
  Serial.begin(115200);

  podklyuchitsyaKWifi();

  server.on("/", otdatGlavnuyuStranicu);
  server.on("/dannye", otdatDannye);
  server.on("/signal", otdatZapisSignala);
  server.onNotFound(otdatNeNajdeno);

  server.begin();
  Serial.println("Veb-server zapushen na portu 80.");
}

void loop() {
  // Принимаем запросы от браузера.
  server.handleClient();

  // И между делом читаем датчик. Задержек в loop быть не должно,
  // иначе мы пропустим удары или страничка начнёт подвисать.
  obrabotatDatchik();
}
