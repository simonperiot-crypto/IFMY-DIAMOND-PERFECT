#pragma once
#include <cstdint>
#include <initializer_list>

namespace dsp
{
    struct Scale
    {
        const char* name;
        uint16_t mask; // bit n = n demi-tons au-dessus de la tonique
    };

    constexpr uint16_t m (std::initializer_list<int> steps)
    {
        uint16_t r = 0;
        for (int s : steps) r = (uint16_t) (r | (1u << s));
        return r;
    }

    // Toutes les gammes proposées dans le plugin
    inline const Scale kScales[] =
    {
        { "Majeure (Ionien)",              m ({ 0, 2, 4, 5, 7, 9, 11 }) },
        { "Mineure naturelle (Eolien)",    m ({ 0, 2, 3, 5, 7, 8, 10 }) },
        { "Mineure harmonique",            m ({ 0, 2, 3, 5, 7, 8, 11 }) },
        { "Mineure melodique",             m ({ 0, 2, 3, 5, 7, 9, 11 }) },
        { "Chromatique",                   0x0FFF },
        { "Dorien",                        m ({ 0, 2, 3, 5, 7, 9, 10 }) },
        { "Phrygien",                      m ({ 0, 1, 3, 5, 7, 8, 10 }) },
        { "Lydien",                        m ({ 0, 2, 4, 6, 7, 9, 11 }) },
        { "Mixolydien",                    m ({ 0, 2, 4, 5, 7, 9, 10 }) },
        { "Locrien",                       m ({ 0, 1, 3, 5, 6, 8, 10 }) },
        { "Pentatonique majeure",          m ({ 0, 2, 4, 7, 9 }) },
        { "Pentatonique mineure",          m ({ 0, 3, 5, 7, 10 }) },
        { "Blues mineur",                  m ({ 0, 3, 5, 6, 7, 10 }) },
        { "Blues majeur",                  m ({ 0, 2, 3, 4, 7, 9 }) },
        { "Majeure harmonique",            m ({ 0, 2, 4, 5, 7, 8, 11 }) },
        { "Phrygien dominant (Hijaz)",     m ({ 0, 1, 4, 5, 7, 8, 10 }) },
        { "Double harmonique (Arabe)",     m ({ 0, 1, 4, 5, 7, 8, 11 }) },
        { "Majeure locrienne (Arabe 2)",   m ({ 0, 2, 4, 5, 6, 8, 10 }) },
        { "Persane",                       m ({ 0, 1, 4, 5, 6, 8, 11 }) },
        { "Hongroise mineure (Tzigane)",   m ({ 0, 2, 3, 6, 7, 8, 11 }) },
        { "Hongroise majeure",             m ({ 0, 3, 4, 6, 7, 9, 10 }) },
        { "Dorien #4 (Ukrainien)",         m ({ 0, 2, 3, 6, 7, 9, 10 }) },
        { "Napolitaine mineure",           m ({ 0, 1, 3, 5, 7, 8, 11 }) },
        { "Napolitaine majeure",           m ({ 0, 1, 3, 5, 7, 9, 11 }) },
        { "Espagnole 8 notes",             m ({ 0, 1, 3, 4, 5, 6, 8, 10 }) },
        { "Lydien dominant",               m ({ 0, 2, 4, 6, 7, 9, 10 }) },
        { "Alteree (Super locrien)",       m ({ 0, 1, 3, 4, 6, 8, 10 }) },
        { "Bebop dominant",                m ({ 0, 2, 4, 5, 7, 9, 10, 11 }) },
        { "Bebop majeur",                  m ({ 0, 2, 4, 5, 7, 8, 9, 11 }) },
        { "Bebop mineur",                  m ({ 0, 2, 3, 4, 5, 7, 9, 10 }) },
        { "Par tons",                      m ({ 0, 2, 4, 6, 8, 10 }) },
        { "Diminuee (1/2 - 1)",            m ({ 0, 1, 3, 4, 6, 7, 9, 10 }) },
        { "Diminuee (1 - 1/2)",            m ({ 0, 2, 3, 5, 6, 8, 9, 11 }) },
        { "Augmentee",                     m ({ 0, 3, 4, 7, 8, 11 }) },
        { "Enigmatique",                   m ({ 0, 1, 4, 6, 8, 10, 11 }) },
        { "Promethee",                     m ({ 0, 2, 4, 6, 9, 10 }) },
        { "Egyptienne",                    m ({ 0, 2, 5, 7, 10 }) },
        { "Hirajoshi (Japon)",             m ({ 0, 2, 3, 7, 8 }) },
        { "In Sen (Japon)",                m ({ 0, 1, 5, 7, 10 }) },
        { "Iwato (Japon)",                 m ({ 0, 1, 5, 6, 10 }) },
        { "Kumoi (Japon)",                 m ({ 0, 2, 3, 7, 9 }) },
        { "Pelog (Bali)",                  m ({ 0, 1, 3, 7, 8 }) },
        { "Accord majeur",                 m ({ 0, 4, 7 }) },
        { "Accord mineur",                 m ({ 0, 3, 7 }) },
        { "Accord maj7",                   m ({ 0, 4, 7, 11 }) },
        { "Accord min7",                   m ({ 0, 3, 7, 10 }) },
        { "Accord 7 (dominant)",           m ({ 0, 4, 7, 10 }) },
        { "Quintes (Robot)",               m ({ 0, 7 }) },
    };

    constexpr int kNumScales = (int) (sizeof (kScales) / sizeof (kScales[0]));

    inline const char* kKeyNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    inline const char* kKeyNamesLong[12] = { "C  (Do)", "C# / Db  (Do#)", "D  (Re)", "D# / Eb  (Re#)", "E  (Mi)", "F  (Fa)",
                                             "F# / Gb  (Fa#)", "G  (Sol)", "G# / Ab  (Sol#)", "A  (La)", "A# / Bb  (La#)", "B  (Si)" };

    // Masque en classes de hauteur absolues (bit 0 = C) pour une tonique donnée
    inline uint16_t absoluteMask (int key, uint16_t scaleMask)
    {
        uint16_t r = 0;
        for (int i = 0; i < 12; ++i)
            if (scaleMask & (1u << i))
                r = (uint16_t) (r | (1u << ((i + key) % 12)));
        return r;
    }
}
