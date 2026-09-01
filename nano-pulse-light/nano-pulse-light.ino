// Пульс на ленту. Arduino Nano + HW827 + IRF520.
//
// Что делает скетч:
//   1. Читает пульс с HW827 на A0.
//   2. В режиме 1 качает яркость ленты в ритм ударов.
//   3. Кнопка на D2: режимы, таймер сна, метроном, стробоскоп.
//   4. По истечении таймера гасит ленту и уходит в сон. Будит та же кнопка.
//
// Подключение:
//   HW827 VCC -> 5V, GND -> GND, S -> A0
//   IRF520 SIG -> D11 (PWM), GND логики общий с Nano
//   Кнопка NC между D2 и GND, резистор не нужен (внутренний подтяг)

#include <avr/sleep.h>
#include <avr/interrupt.h>

#include "config.h"

// --- обработка сигнала пульса ---

float bystroe_srednee = 0;
float bazovaya_liniya = 0;
bool pervoe_chtenie = true;
float sredniy_kvadrat = 0;
bool byli_nizhe_poroga = true;

unsigned long vremya_poslednego_udara = 0;
unsigned long vremya_pervogo_udara = 0;
unsigned long vremya_poslednego_chteniya = 0;

unsigned long promezhutki[SKOLKO_PROMEZHUTKOV_USREDNYAT];
int skolko_promezhutkov_nabrali = 0;
int kuda_pisat_promezhutok = 0;
int propuskov_podryad = 0;

const int RESHENIE_NE_UDAR = 0;
const int RESHENIE_ZAPOMNIT = 1;
const int RESHENIE_PROPUSK = 2;

// Времена ударов для скользящего окна 10 с.
unsigned long vremena_udarov[MAX_UDAROV_V_OKNE];
int skolko_udarov_v_okne = 0;

int pulse_ud_v_minutu = STARTOVYY_PULS;

// Фаза качки ленты от 0 до 1. 0 — максимум яркости, 0.5 — минимум.
// Идём шагами по времени, а не через «миллисекунды % период»:
// иначе при смене пульса яркость резко прыгает, это и есть всплески.
float faza_lenty = 0;

// --- режимы и кнопка ---

int rezhim = REZHIM_PULS;
bool metronom_vklyuchen = false;
bool strob_vklyuchen = false;

bool taymer_vklyuchen = false;
unsigned long vremya_starta_taymera = 0;

int skolko_klikov = 0;
bool knopka_seychas_nazhata = false;
unsigned long vremya_smeny_knopki = 0;
unsigned long vremya_poslednego_klika = 0;
bool zhdyom_eshyo_kliki = false;

unsigned long vremya_poslednego_kadra_lenty = 0;
unsigned long vremya_posledney_otladki = 0;

// Прерывание смены уровня на D2. Само ничего не делает:
// нужно только чтобы процессор проснулся из глубокого сна.
ISR(PCINT2_vect) {
}

int procentVShimm(int procent) {
  if (procent <= 0) {
    return 0;
  }
  if (procent >= 100) {
    return 255;
  }
  return (procent * 255) / 100;
}

int yarkostRezhima(int nomer) {
  if (nomer == REZHIM_20) {
    return YARKOST_REZHIM_2_PROCENT;
  }
  if (nomer == REZHIM_40) {
    return YARKOST_REZHIM_3_PROCENT;
  }
  if (nomer == REZHIM_60) {
    return YARKOST_REZHIM_4_PROCENT;
  }
  if (nomer == REZHIM_80) {
    return YARKOST_REZHIM_5_PROCENT;
  }
  if (nomer == REZHIM_100) {
    return YARKOST_REZHIM_6_PROCENT;
  }
  return 0;
}

void postavitYarkostProcent(int procent) {
  analogWrite(NOZHKA_LENTY, procentVShimm(procent));
}

void pogasitLentu() {
  analogWrite(NOZHKA_LENTY, 0);
  digitalWrite(NOZHKA_LENTY, LOW);
}

// Плавная качка: пик в начале периода, минимум в середине, снова пик в конце.
// dt_ms — сколько миллисекунд прошло с прошлого кадра.
void dyshatLentoy(int min_procent, int max_procent,
                  unsigned long period_ms, unsigned long dt_ms) {
  if (period_ms < 1) {
    period_ms = 1;
  }

  // После сна или долгого пропуска кадра не перескакиваем через полволны.
  if (dt_ms > 100) {
    dt_ms = PERIOD_LENTY_MS;
  }

  faza_lenty += (float)dt_ms / (float)period_ms;
  while (faza_lenty >= 1.0) {
    faza_lenty -= 1.0;
  }

  // cos(0) = 1, это максимум. cos(pi) = -1, это минимум.
  float volna = 0.5 + 0.5 * cos(faza_lenty * 2.0 * PI);
  int procent = min_procent + (int)((max_procent - min_procent) * volna);
  postavitYarkostProcent(procent);
}

unsigned long sredniyPromezhutok() {
  if (skolko_promezhutkov_nabrali == 0) {
    return 0;
  }

  unsigned long summa = 0;
  for (int i = 0; i < skolko_promezhutkov_nabrali; i++) {
    summa += promezhutki[i];
  }
  return summa / skolko_promezhutkov_nabrali;
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

void ochistitOknoUdarov() {
  skolko_udarov_v_okne = 0;
  vremya_pervogo_udara = 0;
}

void sbrositPokazaniya() {
  skolko_promezhutkov_nabrali = 0;
  kuda_pisat_promezhutok = 0;
  propuskov_podryad = 0;
  vremya_poslednego_udara = 0;
  ochistitOknoUdarov();
  // Пульс не трогаем: если палец сняли, лента продолжает
  // дышать в том темпе, который был до этого.
}

void dobavitUdarVOkno(unsigned long seychas) {
  if (skolko_udarov_v_okne < MAX_UDAROV_V_OKNE) {
    vremena_udarov[skolko_udarov_v_okne] = seychas;
    skolko_udarov_v_okne++;
    return;
  }

  // Окно забито - сдвигаем влево и пишем новый удар в конец.
  for (int i = 1; i < MAX_UDAROV_V_OKNE; i++) {
    vremena_udarov[i - 1] = vremena_udarov[i];
  }
  vremena_udarov[MAX_UDAROV_V_OKNE - 1] = seychas;
}

void ubratStaryeUdary(unsigned long granica) {
  int ostalos = 0;
  for (int i = 0; i < skolko_udarov_v_okne; i++) {
    if (vremena_udarov[i] >= granica) {
      vremena_udarov[ostalos] = vremena_udarov[i];
      ostalos++;
    }
  }
  skolko_udarov_v_okne = ostalos;
}

void poschitatPulsePoOknu(unsigned long seychas) {
  if (vremya_pervogo_udara == 0) {
    return;
  }

  unsigned long nachalo_okna;

  // Пока с первого удара не прошло 10 с - окно растёт от первого удара.
  // Потом едет следом за временем как скользящее окно 10 с.
  if (seychas - vremya_pervogo_udara < OKNO_PULSA_MS) {
    nachalo_okna = vremya_pervogo_udara;
  } else {
    nachalo_okna = seychas - OKNO_PULSA_MS;
  }

  ubratStaryeUdary(nachalo_okna);

  // Один удар цифру не даёт. Держимся за то, что уже есть.
  if (skolko_udarov_v_okne < 2) {
    return;
  }

  unsigned long pervyy = vremena_udarov[0];
  unsigned long posledniy = vremena_udarov[skolko_udarov_v_okne - 1];
  unsigned long delta = posledniy - pervyy;
  if (delta < 1) {
    return;
  }

  // Ударов N, промежутков N-1. Так окно не завышает пульс на один удар.
  int novyy_puls = (int)(((skolko_udarov_v_okne - 1) * 60000UL) / delta);

  // Ниже 40 и выше 140 — мусор, оставляем прошлую цифру.
  if (novyy_puls < MIN_PULS || novyy_puls > MAX_PULS) {
    return;
  }

  pulse_ud_v_minutu = novyy_puls;
}

int resheniePoPromezhutku(unsigned long promezhutok) {
  if (promezhutok < MIN_PROMEZHUTOK_MS) {
    return RESHENIE_NE_UDAR;
  }

  if (skolko_promezhutkov_nabrali < 3) {
    return RESHENIE_ZAPOMNIT;
  }

  unsigned long sredniy = sredniyPromezhutok();

  if (promezhutok < sredniy * MIN_DOLYA_OT_SREDNEGO) {
    return RESHENIE_NE_UDAR;
  }

  if (promezhutok > sredniy * MAX_DOLYA_OT_SREDNEGO) {
    return RESHENIE_PROPUSK;
  }

  return RESHENIE_ZAPOMNIT;
}

void zaregistrirovatUdar(unsigned long seychas) {
  if (vremya_pervogo_udara == 0) {
    vremya_pervogo_udara = seychas;
  }

  dobavitUdarVOkno(seychas);
  poschitatPulsePoOknu(seychas);

  vremya_poslednego_udara = seychas;
  // Фазу ленты здесь не сбрасываем: резкий прыжок к пику
  // как раз давал всплески мерцания в ритм ударов.
}

void obrabotatDatchik() {
  unsigned long seychas = millis();

  if (seychas - vremya_poslednego_chteniya < PERIOD_CHTENIYA_MS) {
    return;
  }
  vremya_poslednego_chteniya = seychas;

  int syroy_signal = analogRead(NOZHKA_DATCHIKA);

  if (pervoe_chtenie) {
    pervoe_chtenie = false;
    bystroe_srednee = syroy_signal;
    bazovaya_liniya = syroy_signal;
    return;
  }

  bystroe_srednee += (syroy_signal - bystroe_srednee) * ALFA_BYSTROGO;
  bazovaya_liniya += (bystroe_srednee - bazovaya_liniya) * ALFA_BAZOVOY;

  float volna = bystroe_srednee - bazovaya_liniya;
  float sila = sqrt(sredniy_kvadrat);

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

  if (sila < MIN_SILA) {
    if (vremya_pervogo_udara != 0) {
      sbrositPokazaniya();
    }
    byli_nizhe_poroga = true;
    return;
  }

  float porog = sila * POROG_V_SILAH;
  float uroven_vozvrata = sila * VOZVRAT_V_SILAH;

  if (byli_nizhe_poroga && volna > porog) {
    byli_nizhe_poroga = false;

    unsigned long promezhutok = seychas - vremya_poslednego_udara;
    int reshenie = resheniePoPromezhutku(promezhutok);

    if (reshenie != RESHENIE_NE_UDAR) {
      if (promezhutok > MAX_PROMEZHUTOK_MS || vremya_poslednego_udara == 0) {
        sbrositPokazaniya();
        zaregistrirovatUdar(seychas);
      } else if (reshenie == RESHENIE_PROPUSK) {
        propuskov_podryad++;
        if (propuskov_podryad >= SKOLKO_PROPUSKOV_DO_SBROSA) {
          sbrositPokazaniya();
        }
        zaregistrirovatUdar(seychas);
      } else {
        propuskov_podryad = 0;
        zapomnitPromezhutok(promezhutok);
        zaregistrirovatUdar(seychas);
      }
    }
  }

  if (!byli_nizhe_poroga && volna < uroven_vozvrata) {
    byli_nizhe_poroga = true;
  }

  if (vremya_poslednego_udara > 0 &&
      seychas - vremya_poslednego_udara > TAYMAUT_SIGNALA_MS) {
    sbrositPokazaniya();
  }

  poschitatPulsePoOknu(seychas);
}

// Резкий квадрат: половина периода — максимум, половина — ноль.
void strobitLentoy() {
  unsigned long polovina = STROB_PERIOD_MS / 2;
  if (polovina < 1) {
    polovina = 1;
  }

  unsigned long kusok = millis() / polovina;
  if ((kusok % 2) == 0) {
    postavitYarkostProcent(STROB_MAX_PROCENT);
  } else {
    postavitYarkostProcent(STROB_MIN_PROCENT);
  }
}

void obnovitLentu() {
  unsigned long seychas = millis();

  // Стробоскоп обновляем каждый круг loop, иначе фронт вспышки смажется.
  if (strob_vklyuchen) {
    strobitLentoy();
    return;
  }

  if (seychas - vremya_poslednego_kadra_lenty < PERIOD_LENTY_MS) {
    return;
  }

  unsigned long dt_ms = seychas - vremya_poslednego_kadra_lenty;
  vremya_poslednego_kadra_lenty = seychas;

  if (metronom_vklyuchen) {
    dyshatLentoy(METRONOM_MIN_PROCENT, METRONOM_MAX_PROCENT,
                 METRONOM_PERIOD_MS, dt_ms);
    return;
  }

  if (rezhim == REZHIM_VYKL) {
    pogasitLentu();
    return;
  }

  if (rezhim == REZHIM_PULS) {
    int puls = pulse_ud_v_minutu;
    if (puls < 1) {
      puls = STARTOVYY_PULS;
    }
    unsigned long period_ms = 60000UL / puls;
    dyshatLentoy(MIN_YARKOST_PULSA_PROCENT, MAX_YARKOST_PULSA_PROCENT,
                 period_ms, dt_ms);
    return;
  }

  postavitYarkostProcent(yarkostRezhima(rezhim));
}

void sleduyushiyRezhim() {
  rezhim++;
  if (rezhim > POSLEDNIY_REZHIM) {
    rezhim = PERVYY_REZHIM;
  }

  Serial.print("Rezhim: ");
  Serial.println(rezhim);
}

void vklyuchitMetronom() {
  strob_vklyuchen = false;
  metronom_vklyuchen = true;
  Serial.println("Metronom vkl");
}

void vklyuchitStrob() {
  metronom_vklyuchen = false;
  strob_vklyuchen = true;
  Serial.println("Strob vkl");
}

void vklyuchitTaymerSna() {
  taymer_vklyuchen = true;
  vremya_starta_taymera = millis();
  Serial.println("Taymer 10 min");
}

void nachatRabotuSnachala() {
  pervoe_chtenie = true;
  sredniy_kvadrat = 0;
  byli_nizhe_poroga = true;
  vremya_poslednego_udara = 0;
  vremya_poslednego_chteniya = 0;
  sbrositPokazaniya();
  pulse_ud_v_minutu = STARTOVYY_PULS;

  rezhim = REZHIM_PULS;
  metronom_vklyuchen = false;
  strob_vklyuchen = false;
  taymer_vklyuchen = false;
  skolko_klikov = 0;
  zhdyom_eshyo_kliki = false;
  knopka_seychas_nazhata = false;

  faza_lenty = 0;
  vremya_poslednego_kadra_lenty = millis();
}

void zhdatOtpuskaniyaKnopki() {
  while (digitalRead(NOZHKA_KNOPKI) == UROVEN_NAZHATIYA) {
    delay(10);
  }
  delay(ANTIDREBEZG_MS);
}

void uytiVSon() {
  Serial.println("Son. Nazhmi knopku chtoby razbudit.");
  Serial.flush();

  pogasitLentu();

  // АЦП во сне не нужен и зря ест ток.
  ADCSRA &= ~(1 << ADEN);

  // Разрешаем прерывание смены уровня на порту D (там лежит D2).
  PCMSK2 |= (1 << PCINT18);
  PCIFR |= (1 << PCIF2);
  PCICR |= (1 << PCIE2);

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sei();
  sleep_cpu();
  sleep_disable();

  // Проснулись. Прерывание больше не нужно, АЦП снова включаем.
  PCICR &= ~(1 << PCIE2);
  ADCSRA |= (1 << ADEN);

  nachatRabotuSnachala();
  zhdatOtpuskaniyaKnopki();

  Serial.println("Prosypayus, rezhim 1");
}

void proveritTaymerSna() {
  if (!taymer_vklyuchen) {
    return;
  }

  if (millis() - vremya_starta_taymera >= TAYMER_SNA_MS) {
    taymer_vklyuchen = false;
    uytiVSon();
  }
}

void obrabotatKiliki() {
  if (skolko_klikov == 1) {
    // Одинарный клик из метронома/строба только выходит, режим не трогает.
    if (metronom_vklyuchen || strob_vklyuchen) {
      metronom_vklyuchen = false;
      strob_vklyuchen = false;
      Serial.println("Nalozhenie vykl");
    } else {
      sleduyushiyRezhim();
    }
  } else if (skolko_klikov == 2) {
    vklyuchitTaymerSna();
  } else if (skolko_klikov == 3) {
    vklyuchitMetronom();
  } else if (skolko_klikov >= 4) {
    vklyuchitStrob();
  }

  skolko_klikov = 0;
  zhdyom_eshyo_kliki = false;
}

void obrabotatKnopku() {
  unsigned long seychas = millis();
  int uroven = digitalRead(NOZHKA_KNOPKI);
  bool nazhata = (uroven == UROVEN_NAZHATIYA);

  if (nazhata != knopka_seychas_nazhata) {
    if (seychas - vremya_smeny_knopki < ANTIDREBEZG_MS) {
      return;
    }
    vremya_smeny_knopki = seychas;
    knopka_seychas_nazhata = nazhata;

    // Считаем клик в момент отпускания: так дребезг меньше путает пачки.
    if (!nazhata) {
      skolko_klikov++;
      vremya_poslednego_klika = seychas;
      zhdyom_eshyo_kliki = true;
    }
  }

  if (zhdyom_eshyo_kliki &&
      !knopka_seychas_nazhata &&
      seychas - vremya_poslednego_klika >= PAUZA_KLIKOV_MS) {
    obrabotatKiliki();
  }
}

void pechatOtladku() {
  unsigned long seychas = millis();
  if (seychas - vremya_posledney_otladki < PERIOD_OTLADKI_MS) {
    return;
  }
  vremya_posledney_otladki = seychas;

  Serial.print("pulse=");
  Serial.print(pulse_ud_v_minutu);
  Serial.print(" sila=");
  Serial.print(sqrt(sredniy_kvadrat), 1);
  Serial.print(" rezhim=");
  Serial.print(rezhim);
  Serial.print(" metro=");
  Serial.print(metronom_vklyuchen ? 1 : 0);
  Serial.print(" strob=");
  Serial.print(strob_vklyuchen ? 1 : 0);
  Serial.print(" taymer=");
  if (taymer_vklyuchen) {
    unsigned long ostalos_s = (TAYMER_SNA_MS - (seychas - vremya_starta_taymera)) / 1000UL;
    Serial.print(ostalos_s);
    Serial.print("s");
  } else {
    Serial.print("-");
  }
  Serial.println();
}

void setup() {
  pinMode(NOZHKA_LENTY, OUTPUT);
  pogasitLentu();

  pinMode(NOZHKA_KNOPKI, INPUT_PULLUP);
  pinMode(NOZHKA_DATCHIKA, INPUT);

  Serial.begin(115200);
  nachatRabotuSnachala();

  Serial.println("nano-pulse-light");
  Serial.println("Knopka NC: D2-GND. Lenta PWM: D11. Puls: A0.");
}

void loop() {
  obrabotatDatchik();
  obrabotatKnopku();
  obnovitLentu();
  proveritTaymerSna();
  pechatOtladku();
}
