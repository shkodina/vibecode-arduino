from wc_control.names import module_filename, translit_stem


def test_cyrillic_filename_becomes_latin_with_dashes():
    assert translit_stem("журчание ручья в лесу") == "zhurchanie-ruchya-v-lesu"
    assert module_filename("журчание ручья в лесу.mp3") == "zhurchanie-ruchya-v-lesu.wav"


def test_spaces_and_punctuation_collapse_to_dashes():
    assert translit_stem("Bird  01 (mix)") == "bird-01-mix"


def test_latin_name_stays_latin_and_lowercased():
    assert module_filename("Bird01.WAV") == "bird01.wav"


def test_empty_after_strip_falls_back():
    assert module_filename("... .mp3") == "sound.wav"
