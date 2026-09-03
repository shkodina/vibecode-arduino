#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Корпус фонаря nano-pulse-light — обтекаемый, со скруглениями.

Печатные детали (на каждую STL и DAE):
  korpus          основа, ручка сверху, площадка ленты, окна кнопок
  vyemnyy_blok    задняя дверь + лоток аккумулятора
  SF_12045        только габарит батареи, не печатать

Размеры в миллиметрах. Геометрия — SDF + marching cubes, поэтому края
скруглённые, отверстия круглые.
"""

import math
import os
import struct

import numpy as np

# --- аккумулятор SF 12045 ---
AKK_X = 90.0
AKK_Y = 70.0
AKK_Z = 101.0
AKK_Z_S_KLEMMAMI = 107.0

# --- печать PLA, сопло 0.2 мм ---
STENKA = 2.6
DNO = 3.2
ZAZOR_AKK = 2.2
ZAZOR_SKOLZ = 0.7
SKRUG = 12.0          # наружный радиус скругления корпуса
SKRUG_VNUTR = 8.0

# лоток
LOT_STENKA = 2.2
LOT_DNO = 3.0
LOT_VNUTR_X = AKK_X + 2 * ZAZOR_AKK
LOT_VNUTR_Y = AKK_Y + 2 * ZAZOR_AKK
LOT_VNUTR_Z = AKK_Z_S_KLEMMAMI + 6.0
LOT_NARUZH_X = LOT_VNUTR_X + 2 * LOT_STENKA
LOT_NARUZH_Y = LOT_VNUTR_Y + 2 * LOT_STENKA
LOT_NARUZH_Z = LOT_VNUTR_Z + LOT_DNO

# наружный габарит без ручки
W = 124.0
H = 158.0
FRONT = 3.4
VOZDUH = 9.0
DVER = 3.0
D_OPEN = FRONT + VOZDUH + LOT_NARUZH_Y
D = D_OPEN + DVER

LOT_X0 = (W - LOT_NARUZH_X) / 2.0
LOT_Z0 = DNO + 1.0
LOT_Y0 = FRONT + VOZDUH
LOT_Y1 = LOT_Y0 + LOT_NARUZH_Y

POLKA_Z = LOT_Z0 + LOT_NARUZH_Z + 4.0
POLKA_T = 2.6

# лента
KANAL_SHIR = 10.6
KANAL_GLUB = 2.2
KANAL_N = 6
RASSEI = 1.1

# кнопка питания: цилиндр, посадка 14 мм, глубина 12 мм, клеммы ещё 6 мм
PIT_D = 14.0
PIT_POSADKA = 12.0
PIT_KLEMMA = 6.0
PIT_ZAZOR = 0.25

# кнопка режимов: квадрат 13.5 мм, посадка 12 мм, клеммы 6 мм
REZH_STORONA = 13.5
REZH_POSADKA = 12.0
REZH_KLEMMA = 6.0
REZH_ZAZOR = 0.3

# датчик пульса HW827: окно под палец + карман под плату сзади
PULS_OKNO = 16.0
PULS_KARMAN_X = 22.0
PULS_KARMAN_Y = 18.0
PULS_KARMAN_Z = 8.0

# винты M3
M3_SKVOZ = 3.3
M3_SAMOREZ = 2.5
M3_OT_KRAYA = 10.0
BOB_D = 9.0
BOB_H = 8.0

# ручка для переноски (сверху сзади)
RUCHKA_R = 11.0          # толщина трубки хвата
RUCHKA_VYSOTA = 34.0     # насколько торчит над крышкой
RUCHKA_SHIR = 86.0       # ширина между стойками (по осям)
RUCHKA_OT_ZADA = 22.0    # ось хвата от задней грани вперёд

# сетка marching cubes, мм. 0.7 — гладко и ещё считается за минуту.
SHAG = 1.0            # мм, сетка поверхности. Мельче = глаже и дольше.


# --- SDF ---

def sdf_round_box(x, y, z, cx, cy, cz, hx, hy, hz, r):
    px = np.abs(x - cx) - (hx - r)
    py = np.abs(y - cy) - (hy - r)
    pz = np.abs(z - cz) - (hz - r)
    ax = np.maximum(px, 0.0)
    ay = np.maximum(py, 0.0)
    az = np.maximum(pz, 0.0)
    snaruzhi = np.sqrt(ax * ax + ay * ay + az * az)
    vnutri = np.minimum(np.maximum(px, np.maximum(py, pz)), 0.0)
    return snaruzhi + vnutri - r


def sdf_yashchik(x, y, z, x0, y0, z0, dx, dy, dz):
    px = np.abs(x - (x0 + dx / 2.0)) - dx / 2.0
    py = np.abs(y - (y0 + dy / 2.0)) - dy / 2.0
    pz = np.abs(z - (z0 + dz / 2.0)) - dz / 2.0
    ax = np.maximum(px, 0.0)
    ay = np.maximum(py, 0.0)
    az = np.maximum(pz, 0.0)
    snaruzhi = np.sqrt(ax * ax + ay * ay + az * az)
    vnutri = np.minimum(np.maximum(px, np.maximum(py, pz)), 0.0)
    return snaruzhi + vnutri


def sdf_cilindr_z(x, y, z, cx, cy, z0, z1, r):
    """Цилиндр вдоль Z, от z0 до z1."""
    dx = x - cx
    dy = y - cy
    poperek = np.sqrt(dx * dx + dy * dy) - r
    vdol = np.abs(z - (z0 + z1) / 2.0) - abs(z1 - z0) / 2.0
    ax = np.maximum(poperek, 0.0)
    az = np.maximum(vdol, 0.0)
    snaruzhi = np.sqrt(ax * ax + az * az)
    vnutri = np.minimum(np.maximum(poperek, vdol), 0.0)
    return snaruzhi + vnutri


def sdf_cilindr_y(x, y, z, cx, cz, y0, y1, r):
    dx = x - cx
    dz = z - cz
    poperek = np.sqrt(dx * dx + dz * dz) - r
    vdol = np.abs(y - (y0 + y1) / 2.0) - abs(y1 - y0) / 2.0
    ax = np.maximum(poperek, 0.0)
    ay = np.maximum(vdol, 0.0)
    snaruzhi = np.sqrt(ax * ax + ay * ay)
    vnutri = np.minimum(np.maximum(poperek, vdol), 0.0)
    return snaruzhi + vnutri


def sdf_kapsula(x, y, z, ax, ay, az, bx, by, bz, r):
    px = x - ax
    py = y - ay
    pz = z - az
    bax = bx - ax
    bay = by - ay
    baz = bz - az
    ba2 = bax * bax + bay * bay + baz * baz
    ba2 = np.where(ba2 < 1e-9, 1.0, ba2)
    t = np.clip((px * bax + py * bay + pz * baz) / ba2, 0.0, 1.0)
    dx = px - bax * t
    dy = py - bay * t
    dz = pz - baz * t
    return np.sqrt(dx * dx + dy * dy + dz * dz) - r


def sdf_kvadrat_z(x, y, z, cx, cy, z0, z1, storona, skr=1.2):
    """Скруглённый квадрат вдоль Z (кнопка режимов)."""
    hx = storona / 2.0
    return sdf_round_box(
        x, y, z,
        cx, cy, (z0 + z1) / 2.0,
        hx, hx, abs(z1 - z0) / 2.0,
        skr
    )


def smin(a, b, k=4.0):
    """Мягкое объединение — обтекаемые стыки."""
    h = np.clip(0.5 + 0.5 * (b - a) / k, 0.0, 1.0)
    return (1.0 - h) * b + h * a - k * h * (1.0 - h)


def smax(a, b, k=3.0):
    """Мягкое вычитание / пересечение."""
    return -smin(-a, -b, k)


def vinty_pozicii():
    """Четыре винта M3 по углам задней грани."""
    xs = (M3_OT_KRAYA, W - M3_OT_KRAYA)
    zs = (M3_OT_KRAYA, H - M3_OT_KRAYA)
    return [(x, z) for x in xs for z in zs]


def knopki_pozicii():
    """
    Все органы на верхней грани, спереди от ручки.
    Питание слева, режимы по центру, пульс справа — палец кладётся сверху.
    """
    z_verh = H
    y_ryad = 28.0
    pit = (28.0, y_ryad, z_verh)
    rezh = (58.0, y_ryad, z_verh)
    puls = (96.0, y_ryad, z_verh)
    return pit, rezh, puls


def sdf_ruchka(x, y, z):
    """Дуговая ручка сверху сзади: две стойки и перекладина."""
    y_osi = D - RUCHKA_OT_ZADA
    z_niz = H - 4.0
    z_verh = H + RUCHKA_VYSOTA
    x_lev = (W - RUCHKA_SHIR) / 2.0
    x_prav = (W + RUCHKA_SHIR) / 2.0
    stoyka_l = sdf_kapsula(x, y, z, x_lev, y_osi, z_niz, x_lev, y_osi, z_verh, RUCHKA_R)
    stoyka_p = sdf_kapsula(x, y, z, x_prav, y_osi, z_niz, x_prav, y_osi, z_verh, RUCHKA_R)
    perekl = sdf_kapsula(x, y, z, x_lev, y_osi, z_verh, x_prav, y_osi, z_verh, RUCHKA_R)
    return smin(smin(stoyka_l, stoyka_p, 6.0), perekl, 6.0)


def sdf_kanaly_lenty(x, y, z):
    """Шесть горизонтальных каналов, не прорезают рассеиватель."""
    pole_z0 = 18.0
    pole_z1 = H - 22.0
    pole_h = pole_z1 - pole_z0
    n_rebro = KANAL_N + 1
    mesto_ryober = pole_h - KANAL_N * KANAL_SHIR
    rebro = mesto_ryober / n_rebro
    x0 = STENKA + 8.0
    dx = W - 2 * (STENKA + 8.0)
    d = None
    z_tek = pole_z0 + rebro
    for _ in range(KANAL_N):
        kanal = sdf_yashchik(x, y, z, x0, RASSEI, z_tek, dx, KANAL_GLUB, KANAL_SHIR)
        d = kanal if d is None else np.minimum(d, kanal)
        z_tek += KANAL_SHIR + rebro
    return d


def sdf_korpus(x, y, z):
    cx, cy, cz = W / 2.0, D / 2.0, H / 2.0
    hx, hy, hz = W / 2.0, D / 2.0, H / 2.0

    naruzh = sdf_round_box(x, y, z, cx, cy, cz, hx, hy, hz, SKRUG)
    # внутренняя полость чуть сдвинута вверх от дна
    vnutr = sdf_round_box(
        x, y, z,
        W / 2.0,
        (D_OPEN + FRONT) / 2.0,
        DNO + (H - DNO - 2.8) / 2.0,
        (W - 2 * STENKA) / 2.0,
        (D_OPEN - FRONT) / 2.0 + 1.0,
        (H - DNO - 2.8) / 2.0,
        SKRUG_VNUTR
    )
    obolochka = smax(naruzh, -vnutr, 1.2)

    # открытая спина под выемный блок
    spina = sdf_yashchik(
        x, y, z,
        LOT_X0 - ZAZOR_SKOLZ,
        D_OPEN - 0.4,
        LOT_Z0 - ZAZOR_SKOLZ,
        LOT_NARUZH_X + 2 * ZAZOR_SKOLZ,
        DVER + 8.0,
        LOT_NARUZH_Z + 2 * ZAZOR_SKOLZ
    )
    obolochka = smax(obolochka, -spina, 1.0)

    # верхний проём чуть шире, чтобы блок выходил
    proem_verh = sdf_yashchik(
        x, y, z,
        STENKA + 1.0,
        D_OPEN - 0.2,
        POLKA_Z - 1.0,
        W - 2 * STENKA - 2.0,
        DVER + 6.0,
        H - POLKA_Z - 2.0
    )
    obolochka = smax(obolochka, -proem_verh, 1.0)

    # каналы ленты
    obolochka = smax(obolochka, -sdf_kanaly_lenty(x, y, z), 0.6)

    # щель в полке под провода клемм — полка это часть оболочки;
    # вырезаем канал из полости уже открытой спиной. Дополнительно
    # дырка над серединой лотка:
    shchel = sdf_yashchik(
        x, y, z,
        W / 2.0 - 11.0,
        LOT_Y0 + LOT_NARUZH_Y / 2.0 - 8.0,
        POLKA_Z - 1.0,
        22.0, 16.0, POLKA_T + 4.0
    )
    obolochka = smax(obolochka, -shchel, 0.8)

    # ручка
    obolochka = smin(obolochka, sdf_ruchka(x, y, z), 7.0)

    pit, rezh, puls = knopki_pozicii()

    # бобышка + отверстие питания (цилиндр 14.2, глубина посадки 12,
    # за бобышкой воздух под клеммы 6 мм)
    pit_r = PIT_D / 2.0 + PIT_ZAZOR
    bob_pit = sdf_cilindr_z(
        x, y, z, pit[0], pit[1],
        H - PIT_POSADKA - 1.0, H + 1.0,
        pit_r + 3.2
    )
    obolochka = smin(obolochka, bob_pit, 3.0)
    dyr_pit = sdf_cilindr_z(
        x, y, z, pit[0], pit[1],
        H - PIT_POSADKA - PIT_KLEMMA - 1.0, H + 2.0,
        pit_r
    )
    obolochka = smax(obolochka, -dyr_pit, 0.8)

    # бобышка + квадратное отверстие режимов
    rezh_h = REZH_STORONA / 2.0 + REZH_ZAZOR
    bob_rezh = sdf_round_box(
        x, y, z,
        rezh[0], rezh[1], H - REZH_POSADKA / 2.0,
        rezh_h + 3.2, rezh_h + 3.2, REZH_POSADKA / 2.0 + 1.0,
        2.0
    )
    obolochka = smin(obolochka, bob_rezh, 3.0)
    dyr_rezh = sdf_kvadrat_z(
        x, y, z, rezh[0], rezh[1],
        H - REZH_POSADKA - REZH_KLEMMA - 1.0, H + 2.0,
        REZH_STORONA + 2 * REZH_ZAZOR,
        1.0
    )
    obolochka = smax(obolochka, -dyr_rezh, 0.8)

    # окно пульса: тонкое дно 1 мм не делаем — сквозное, палец видит сенсор.
    # Сенсор сидит в кармане с внутренней стороны.
    bob_puls = sdf_cilindr_z(
        x, y, z, puls[0], puls[1],
        H - 10.0, H + 1.0,
        PULS_OKNO / 2.0 + 4.0
    )
    obolochka = smin(obolochka, bob_puls, 3.0)
    okno_puls = sdf_cilindr_z(
        x, y, z, puls[0], puls[1],
        H - 14.0, H + 2.0,
        PULS_OKNO / 2.0 + 0.2
    )
    obolochka = smax(obolochka, -okno_puls, 0.7)
    karman = sdf_yashchik(
        x, y, z,
        puls[0] - PULS_KARMAN_X / 2.0,
        puls[1] - PULS_KARMAN_Y / 2.0,
        H - 12.0 - PULS_KARMAN_Z,
        PULS_KARMAN_X, PULS_KARMAN_Y, PULS_KARMAN_Z + 2.0
    )
    obolochka = smax(obolochka, -karman, 0.8)

    # бобышки и отверстия винтов на задней рамке
    for vx, vz in vinty_pozicii():
        bob = sdf_cilindr_y(
            x, y, z, vx, vz,
            D_OPEN - BOB_H, D_OPEN + 0.6,
            BOB_D / 2.0
        )
        obolochka = smin(obolochka, bob, 2.0)
        dyr = sdf_cilindr_y(
            x, y, z, vx, vz,
            D_OPEN - BOB_H - 1.0, D_OPEN + 2.0,
            M3_SAMOREZ / 2.0
        )
        obolochka = smax(obolochka, -dyr, 0.5)

    return obolochka


def sdf_vyemnyy(x, y, z):
    """Дверь со скруглением + лоток + ручка-петля + отверстия M3."""
    # дверная плита, скруглённая как зад корпуса
    dver = sdf_round_box(
        x, y, z,
        W / 2.0, D_OPEN + DVER / 2.0, H / 2.0,
        W / 2.0 - 0.4, DVER / 2.0, H / 2.0 - 0.4,
        min(SKRUG, 10.0)
    )

    # лоток
    lot_nar = sdf_yashchik(
        x, y, z,
        LOT_X0, LOT_Y0, LOT_Z0,
        LOT_NARUZH_X, LOT_NARUZH_Y, LOT_NARUZH_Z
    )
    lot_vnut = sdf_yashchik(
        x, y, z,
        LOT_X0 + LOT_STENKA,
        LOT_Y0 + LOT_STENKA,
        LOT_Z0 + LOT_DNO,
        LOT_VNUTR_X, LOT_VNUTR_Y, LOT_VNUTR_Z + 4.0
    )
    lotok = smax(lot_nar, -lot_vnut, 0.8)

    blok = smin(dver, lotok, 3.0)

    # петля сзади, чтобы тащить блок (не путать с ручкой переноски)
    y_r = D_OPEN + DVER
    petlya = sdf_kapsula(
        x, y, z,
        W / 2.0 - 16.0, y_r + 9.0, 68.0,
        W / 2.0 + 16.0, y_r + 9.0, 68.0,
        7.0
    )
    noga1 = sdf_kapsula(
        x, y, z,
        W / 2.0 - 16.0, y_r - 1.0, 68.0,
        W / 2.0 - 16.0, y_r + 9.0, 68.0,
        7.0
    )
    noga2 = sdf_kapsula(
        x, y, z,
        W / 2.0 + 16.0, y_r - 1.0, 68.0,
        W / 2.0 + 16.0, y_r + 9.0, 68.0,
        7.0
    )
    blok = smin(blok, smin(smin(petlya, noga1, 4.0), noga2, 4.0), 4.0)

    # сквозные отверстия M3 в двери
    for vx, vz in vinty_pozicii():
        dyr = sdf_cilindr_y(
            x, y, z, vx, vz,
            D_OPEN - 1.0, D_OPEN + DVER + 16.0,
            M3_SKVOZ / 2.0
        )
        blok = smax(blok, -dyr, 0.4)

    return blok


def sdf_akkumulyator(x, y, z):
    x0 = LOT_X0 + LOT_STENKA + ZAZOR_AKK
    y0 = LOT_Y0 + LOT_STENKA + ZAZOR_AKK
    z0 = LOT_Z0 + LOT_DNO + ZAZOR_AKK
    return sdf_yashchik(x, y, z, x0, y0, z0, AKK_X, AKK_Y, AKK_Z)


# --- marching tetrahedra: полные 16 случаев, без дырявой таблицы ---

def treug_normal(a, b, c):
    ux, uy, uz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
    vx, vy, vz = c[0] - a[0], c[1] - a[1], c[2] - a[2]
    nx = uy * vz - uz * vy
    ny = uz * vx - ux * vz
    nz = ux * vy - uy * vx
    dlina = math.sqrt(nx * nx + ny * ny + nz * nz) or 1.0
    return (nx / dlina, ny / dlina, nz / dlina)


def interpol(p1, v1, p2, v2):
    if abs(v1) < 1e-8:
        return p1
    if abs(v2) < 1e-8:
        return p2
    if abs(v1 - v2) < 1e-8:
        return p1
    t = v1 / (v1 - v2)
    return (
        p1[0] + t * (p2[0] - p1[0]),
        p1[1] + t * (p2[1] - p1[1]),
        p1[2] + t * (p2[2] - p1[2]),
    )


def tet_treug(p, v):
    """Четыре угла тетраэдра -> список треугольников."""
    idx = 0
    for i in range(4):
        if v[i] < 0:
            idx |= (1 << i)
    if idx == 0 or idx == 15:
        return []

    def e(a, b):
        return interpol(p[a], v[a], p[b], v[b])

    # один угол внутри
    odin = {
        1: (e(0, 1), e(0, 2), e(0, 3)),
        2: (e(1, 0), e(1, 3), e(1, 2)),
        4: (e(2, 0), e(2, 1), e(2, 3)),
        8: (e(3, 0), e(3, 2), e(3, 1)),
    }
    if idx in odin:
        return [odin[idx]]

    # три угла внутри = дополнение одного, обход наоборот
    tri = {
        14: odin[1],  # не 0
        13: odin[2],
        11: odin[4],
        7: odin[8],
    }
    if idx in tri:
        a, b, c = tri[idx]
        return [(a, c, b)]

    # два угла внутри: четырёхугольник = два треугольника
    dva = {
        3: (e(0, 2), e(0, 3), e(1, 3), e(1, 2)),   # 0 и 1
        5: (e(0, 1), e(0, 3), e(2, 3), e(2, 1)),   # 0 и 2
        6: (e(1, 0), e(1, 3), e(2, 3), e(2, 0)),   # 1 и 2
        9: (e(0, 1), e(0, 2), e(3, 2), e(3, 1)),   # 0 и 3
        10: (e(1, 0), e(1, 2), e(3, 2), e(3, 0)),  # 1 и 3
        12: (e(2, 0), e(2, 1), e(3, 1), e(3, 0)),  # 2 и 3
    }
    q = dva[idx]
    return [(q[0], q[1], q[2]), (q[0], q[2], q[3])]


def marching_cubes(sdf_fn, xmin, xmax, ymin, ymax, zmin, zmax, shag):
    xs = np.arange(xmin, xmax + shag * 0.5, shag, dtype=np.float32)
    ys = np.arange(ymin, ymax + shag * 0.5, shag, dtype=np.float32)
    zs = np.arange(zmin, zmax + shag * 0.5, shag, dtype=np.float32)
    print("  setka %d x %d x %d" % (len(xs), len(ys), len(zs)), flush=True)
    sdf = sdf_fn(
        xs[:, None, None],
        ys[None, :, None],
        zs[None, None, :],
    ).astype(np.float32)
    print("  sdf poschitan", flush=True)

    c0 = sdf[0:-1, 0:-1, 0:-1]
    c1 = sdf[1:, 0:-1, 0:-1]
    c2 = sdf[1:, 1:, 0:-1]
    c3 = sdf[0:-1, 1:, 0:-1]
    c4 = sdf[0:-1, 0:-1, 1:]
    c5 = sdf[1:, 0:-1, 1:]
    c6 = sdf[1:, 1:, 1:]
    c7 = sdf[0:-1, 1:, 1:]
    n_in = (
        (c0 < 0).astype(np.uint8) + (c1 < 0).astype(np.uint8)
        + (c2 < 0).astype(np.uint8) + (c3 < 0).astype(np.uint8)
        + (c4 < 0).astype(np.uint8) + (c5 < 0).astype(np.uint8)
        + (c6 < 0).astype(np.uint8) + (c7 < 0).astype(np.uint8)
    )
    aktiv = np.argwhere((n_in > 0) & (n_in < 8))
    print("  kubov na poverhnosti %d" % len(aktiv), flush=True)

    # шесть тетраэдров куба, общая диагональ 0-6
    tety = (
        (0, 1, 2, 6),
        (0, 2, 3, 6),
        (0, 3, 7, 6),
        (0, 7, 4, 6),
        (0, 4, 5, 6),
        (0, 5, 1, 6),
    )

    mesh = []
    for i, j, k in aktiv:
        p8 = [
            (xs[i], ys[j], zs[k]),
            (xs[i + 1], ys[j], zs[k]),
            (xs[i + 1], ys[j + 1], zs[k]),
            (xs[i], ys[j + 1], zs[k]),
            (xs[i], ys[j], zs[k + 1]),
            (xs[i + 1], ys[j], zs[k + 1]),
            (xs[i + 1], ys[j + 1], zs[k + 1]),
            (xs[i], ys[j + 1], zs[k + 1]),
        ]
        v8 = [
            float(sdf[i, j, k]),
            float(sdf[i + 1, j, k]),
            float(sdf[i + 1, j + 1, k]),
            float(sdf[i, j + 1, k]),
            float(sdf[i, j, k + 1]),
            float(sdf[i + 1, j, k + 1]),
            float(sdf[i + 1, j + 1, k + 1]),
            float(sdf[i, j + 1, k + 1]),
        ]
        for a, b, c, d in tety:
            mesh.extend(tet_treug(
                (p8[a], p8[b], p8[c], p8[d]),
                (v8[a], v8[b], v8[c], v8[d]),
            ))
    return mesh


def pisat_stl(put, mesh):
    n = len(mesh)
    with open(put, "wb") as f:
        zag = b"nano-pulse-light korpus"
        f.write(zag[:80].ljust(80, b"\0"))
        f.write(struct.pack("<I", n))
        for a, b, c in mesh:
            nx, ny, nz = treug_normal(a, b, c)
            f.write(struct.pack("<3f", nx, ny, nz))
            f.write(struct.pack("<3f", *a))
            f.write(struct.pack("<3f", *b))
            f.write(struct.pack("<3f", *c))
            f.write(struct.pack("<H", 0))


def pisat_dae_odin(put, imya, mesh):
    """DAE с общими вершинами — иначе файл раздувается в сотни мегабайт."""
    mapa = {}
    pozy = []
    indeksy = []

    def indeks(p):
        klyuch = (round(p[0] * 50.0), round(p[1] * 50.0), round(p[2] * 50.0))
        nomer = mapa.get(klyuch)
        if nomer is None:
            nomer = len(mapa)
            mapa[klyuch] = nomer
            pozy.extend([p[0] * 0.001, p[1] * 0.001, p[2] * 0.001])
        return nomer

    for a, b, c in mesh:
        indeksy.extend((indeks(a), indeks(b), indeks(c)))

    pozy_txt = " ".join("%.5f" % v for v in pozy)
    idx_txt = " ".join(str(i) for i in indeksy)
    n_tri = len(mesh)
    n_pos = len(pozy) // 3
    xml = f"""<?xml version="1.0" encoding="utf-8"?>
<COLLADA xmlns="http://www.collada.org/2005/11/COLLADASchema" version="1.4.1">
  <asset>
    <contributor>
      <authoring_tool>nano-pulse-light sdelat_korpus.py</authoring_tool>
    </contributor>
    <unit name="meter" meter="1"/>
    <up_axis>Z_UP</up_axis>
  </asset>
  <library_geometries>
    <geometry id="{imya}-geom" name="{imya}">
      <mesh>
        <source id="{imya}-pos">
          <float_array id="{imya}-pos-array" count="{n_pos * 3}">{pozy_txt}</float_array>
          <technique_common>
            <accessor source="#{imya}-pos-array" count="{n_pos}" stride="3">
              <param name="X" type="float"/>
              <param name="Y" type="float"/>
              <param name="Z" type="float"/>
            </accessor>
          </technique_common>
        </source>
        <vertices id="{imya}-vtx">
          <input semantic="POSITION" source="#{imya}-pos"/>
        </vertices>
        <triangles count="{n_tri}">
          <input semantic="VERTEX" source="#{imya}-vtx" offset="0"/>
          <p>{idx_txt}</p>
        </triangles>
      </mesh>
    </geometry>
  </library_geometries>
  <library_visual_scenes>
    <visual_scene id="Scene" name="Scene">
      <node id="{imya}" name="{imya}">
        <instance_geometry url="#{imya}-geom"/>
      </node>
    </visual_scene>
  </library_visual_scenes>
  <scene>
    <instance_visual_scene url="#Scene"/>
  </scene>
</COLLADA>
"""
    with open(put, "w", encoding="utf-8") as f:
        f.write(xml)


def sdf_yashchik_akk(x, y, z):
    return sdf_akkumulyator(x, y, z)


def main():
    papka = os.path.dirname(os.path.abspath(__file__))
    stl_papka = os.path.join(papka, "stl")
    dae_papka = os.path.join(papka, "dae")
    os.makedirs(stl_papka, exist_ok=True)
    os.makedirs(dae_papka, exist_ok=True)

    zapas = 20.0
    print("Korpus...", flush=True)
    mesh_k = marching_cubes(
        sdf_korpus,
        -zapas, W + zapas,
        -zapas, D + RUCHKA_R + 16.0,
        -zapas, H + RUCHKA_VYSOTA + RUCHKA_R + 8.0,
        SHAG,
    )
    print("  treugolnikov %d" % len(mesh_k))

    print("Vyemnyy blok...", flush=True)
    mesh_v = marching_cubes(
        sdf_vyemnyy,
        -4.0, W + 4.0,
        LOT_Y0 - 4.0, D + 24.0,
        -4.0, H + 4.0,
        SHAG,
    )
    print("  treugolnikov %d" % len(mesh_v))

    print("Akkumulyator (ne pechatat)...", flush=True)
    mesh_a = marching_cubes(
        sdf_yashchik_akk,
        LOT_X0 - 2, LOT_X0 + LOT_NARUZH_X + 2,
        LOT_Y0 - 2, LOT_Y1 + 2,
        LOT_Z0 - 2, LOT_Z0 + LOT_NARUZH_Z + 2,
        1.2,
    )

    chasti = (
        ("korpus", mesh_k),
        ("vyemnyy_blok", mesh_v),
        ("SF_12045_ne_pechatat", mesh_a),
    )
    for imya, mesh in chasti:
        pisat_stl(os.path.join(stl_papka, imya + ".stl"), mesh)
        pisat_dae_odin(os.path.join(dae_papka, imya + ".dae"), imya, mesh)
        print("  zapisal %s  (%d tri)" % (imya, len(mesh)))

    pit, rezh, puls = knopki_pozicii()
    print()
    print("Gabarit bez ruchki: %.1f x %.1f x %.1f mm" % (W, H, D))
    print("Ruchka +%.1f mm sverhu" % (RUCHKA_VYSOTA + RUCHKA_R))
    print("Pitanie  D=%.1f  posadka 12 mm  xy=%.1f,%.1f" % (PIT_D, pit[0], pit[1]))
    print("Rezhimy  kvadr %.1f  posadka 12 mm  xy=%.1f,%.1f" % (REZH_STORONA, rezh[0], rezh[1]))
    print("Puls okno D=%.1f  xy=%.1f,%.1f" % (PULS_OKNO, puls[0], puls[1]))
    print("Vinty M3: %s" % vinty_pozicii())
    print("Gotovo:", papka)


if __name__ == "__main__":
    main()
