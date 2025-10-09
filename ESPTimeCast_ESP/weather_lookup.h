#ifndef WEATHER_LOOKUP_H
#define WEATHER_LOOKUP_H

typedef struct {
    const char* lang;
    const char* descriptions[14]; // 14 types
} WeatherCodeMapping;

typedef struct {
    const char* lang;
    struct {
        const char* clear;
        const char* partly_cloudy;
        const char* cloudy;
        const char* overcast;
        const char* fog;
        const char* drizzle;
        const char* rain;
        const char* heavy_rain;
        const char* snow;
        const char* showers;
        const char* snow_showers;
        const char* thunderstorm;
        const char* hail;
        const char* mist;
        const char* light;
        const char* moderate;
        const char* heavy;
        const char* broken_clouds;
        const char* scattered_clouds;
        const char* few_clouds;
    } terms;
} WeatherTermsMapping;

enum WeatherCode {
    WC_CLEAR = 0,           // 0
    WC_PARTLY_CLOUDY = 1,   // 1-2
    WC_CLOUDY = 2,          // 3
    WC_FOG = 3,             // 45,48
    WC_DRIZZLE = 4,         // 51,53,55
    WC_RAIN = 5,            // 61,63
    WC_HEAVY_RAIN = 6,      // 65
    WC_SNOW = 7,            // 71,73,75,77
    WC_SHOWERS = 8,         // 80,81,82
    WC_SNOW_SHOWERS = 9,    // 85,86
    WC_THUNDERSTORM = 10,   // 95
    WC_THUNDERSTORM_HAIL = 11, // 96,99
    WC_UNKNOWN = 12,
    WC_SNOW_GRAINS = 13     // 77
};

const WeatherCodeMapping weather_code_mappings[] = {
    { "en", { "CLEAR", "PARTLY CLOUDY", "CLOUDY", "FOG", "DRIZZLE", "RAIN", "HEAVY RAIN", "SNOW", "SHOWERS", "SNOW SHOWERS", "THUNDERSTORM", "THUNDERSTORM HAIL", "UNKNOWN", "SNOW GRAINS" } },
    { "af", { "HELDER", "GEDEELTELIK BEWOLK", "BEWOLK", "MIS", "MOTREEN", "REEN", "SWAAR REEN", "SNEEU", "BUIE", "SNEEUBUIE", "DONDERSTORM", "DONDERSTORM HAEL", "ONBEKEND", "SNEEU" } },
    { "hr", { "VEDRO", "DJELOMICNO OBLACNO", "OBLACNO", "MAGLA", "ROSENJE", "KISA", "JAKA KISA", "SNIJEG", "PLJUSKOVI", "SNJEZNI PLJUSKOVI", "GRMLJAVINA", "GRMLJAVINA S TUCOM", "NEPOZNATO", "SNIJEG" } },
    { "cs", { "JASNO", "POLOJASNO", "ZATAZENO", "MLHA", "MRHOLENI", "DEST", "SILNY DEST", "SNEZENI", "PREHANKY", "SNEHOVE PREHANKY", "BOURKA", "BOURKA S KRUPOBITIM", "NEZNAME", "SNEZENI" } },
    { "da", { "KLART", "DELVIS SKYET", "OVERSKYET", "TAGE", "STOVREGN", "REGN", "KRAFTIG REGN", "SNE", "BYGER", "SNEBYGER", "TORDENVEJR", "TORDENVEJR MED HAGL", "UKENDT", "SNE" } },
    { "nl", { "HELDER", "DEELS BEWOLKT", "BEWOLKT", "MIST", "MOTREGEN", "REGEN", "ZWARE REGEN", "SNEEUW", "BUIEN", "SNEEUWBUIEN", "ONWEER", "ONWEER MET HAGEL", "ONBEKEND", "SNEEUW" } },
    { "eo", { "KLARA", "PARTE NUBA", "NUBA", "NEBULO", "PLUVETO", "PLUVO", "FORTA PLUVO", "NEGO", "PLUVEGOJ", "NEGPLUVEGOJ", "FULMOTONDRO", "FULMOTONDRO KUN HAJLO", "NEKONATA", "NEGO" } },
    { "et", { "SELGE", "OSALISELT PILVES", "PILVES", "UDU", "UDUVIHM", "VIHM", "TUGEV VIHM", "LUMI", "HOOVIHM", "LUMESADU", "AIKE", "AIKE RAHEGA", "TEADMATA", "LUMI" } },
    { "fi", { "SELKEA", "PUOLIPILVINEN", "PILVINEN", "SUMU", "TIHKUSADE", "SADE", "RANKKASADE", "LUMISADE", "SADEKUUROT", "LUMIKUUROT", "UKKONEN", "UKKONEN JA RAKEITA", "TUNTEMATON", "LUMISADE" } },
    { "fr", { "CLAIR", "PARTIELLEMENT NUAGEUX", "COUVERT", "BROUILLARD", "BRUINE", "PLUIE", "FORTE PLUIE", "NEIGE", "AVERSES", "AVERSES DE NEIGE", "ORAGE", "ORAGE AVEC GRELE", "INCONNU", "NEIGE" } },
    { "de", { "KLAR", "TEILWEISE BEWOLKT", "BEWOLKT", "NEBEL", "NIESELREGEN", "REGEN", "STARKREGEN", "SCHNEE", "SCHAUER", "SCHNEESCHAUER", "GEWITTER", "GEWITTER MIT HAGEL", "UNBEKANNT", "SCHNEE" } },
    { "hu", { "TISZTA", "RESZBEN FELHOS", "FELHOS", "KOD", "SZITALAS", "ESO", "EROS ESO", "HAVAZAS", "ZAPOROK", "HOZAPOROK", "ZIVATAR", "ZIVATAR JEGESSOVEL", "ISMERETLEN", "HAVAZAS" } },
    { "it", { "SERENO", "PARZIALMENTE NUVOLOSO", "COPERTO", "NEBBIA", "PIOVIGGINE", "PIOGGIA", "PIOGGIA FORTE", "NEVE", "ROVESCI", "ROVESCI DI NEVE", "TEMPORALE", "TEMPORALE CON GRANDINE", "SCONOSCIUTO", "NEVE" } },
    { "ga", { "GLAN", "BREAC SCAMALLACH", "SCAMALLACH", "CEO", "CEOBHRAN", "BAISTEACH", "BAISTEACH THROM", "SNEACHTA", "CEATHANNA", "CITH SNEACHTA", "TOIRNEACH", "TOIRNEACH LE CLOCH", "ANAITHNID", "SNEACHTA" } },
    { "ja", { "HARE", "BUBUN KUMORI", "KUMORI", "KIRI", "KIRISAME", "AME", "GOUU", "YUKI", "NIWAKAME", "NIWAKA YUKI", "RAIUU", "RAIUU HYOU", "FUMEI", "YUKI" } },
    { "lv", { "SKAIDRS", "DALEJII MAKONAINS", "APMACIES", "MIGLA", "SMIDZINASANA", "LIETUS", "STIPRS LIETUS", "SNIEGS", "LIETUSGAZE", "SNIEGA GAZES", "PERKONS", "PERKONS AR KRUSU", "NEZINAMS", "SNIEGS" } },
    { "lt", { "GIEDRA", "DEBESUOTA SU PRAGIEDRULIIAIS", "DEBESUOTA", "RUKAS", "DULKSNA", "LIETUS", "SMARKUS LIETUS", "SNIEGAS", "LIUTYS", "SNIEGO LIUTYS", "PERKUINIJA", "PERKUINIJA SU KRUSA", "NEZINOMA", "SNIEGAS" } },
    { "no", { "KLART", "DELVIS SKYET", "OVERSKYET", "TAKE", "YR", "REGN", "KRAFTIG REGN", "SNO", "REGNSKURER", "SNOBYGER", "TORDENVEER", "TORDENVEER MED HAGL", "UKJENT", "SNO" } },
    { "pl", { "BEZCHMURNIE", "CZESCIOWE ZACHMURZENIE", "POCHMURNO", "MGLA", "MZAWKA", "DESZCZ", "ULEWNY DESZCZ", "SNIEG", "PRZELOTNE OPADY", "PRZELOTNY SNIEG", "BURZA", "BURZA Z GRADEM", "NIEZNANE", "SNIEG" } },
    { "pt", { "LIMPO", "PARCIALMENTE NUBLADO", "NUBLADO", "NEVOEIRO", "CHUVISCO", "CHUVA", "CHUVA FORTE", "NEVE", "PANCADAS", "PANCADAS DE NEVE", "TROVOADA", "TROVOADA COM GRANIZO", "DESCONHECIDO", "NEVE" } },
    { "ro", { "SENIN", "PARTIAL NOROS", "INNORAT", "CEATA", "BURNITA", "PLOAIE", "PLOAIE TORENTIALA", "NINSOARE", "AVERSE", "AVERSE DE NINSOARE", "FURTUNA", "FURTUNA CU GRINDINA", "NECUNOSCUT", "NINSOARE" } },
    { "ru", { "YASNO", "MALOOOBLACHNO", "OBLACHNO", "TUMAN", "MORROS", "DOZHD", "SILNYY DOZHD", "SNEG", "LIVNI", "SNEGOPADY", "GROZA", "GROZA S GRADOM", "NEIZVESTNO", "SNEG" } },
    { "sr", { "VEDRO", "DELIMICNO OBLACNO", "OBLACNO", "MAGLA", "ROSENJE", "KISA", "JAKA KISA", "SNEG", "PLJUSKOVI", "SNEZNI PLJUSKOVI", "GRMLJAVINA", "GRMLJAVINA SA GRADOM", "NEPOZNATO", "SNEG" } },
    { "sk", { "JASNO", "POLOJASNO", "ZAMRACENE", "HMLA", "MRHOLENIE", "DAZD", "SILNY DAZD", "SNEZENIE", "PREHANKY", "SNEHOVE PREHANKY", "BURKA", "BURKA S KRUPOBITIM", "NEZNAME", "SNEZENIE" } },
    { "sl", { "JASNO", "DELNO OBLACNO", "OBLACNO", "MEGLA", "ROSENJE", "DEZ", "MOCEN DEZ", "SNEZENJE", "PLOHE", "SNEZNE PLOHE", "NEVIHTA", "NEVIHTA S TOCO", "NEZNANO", "SNEZENJE" } },
    { "es", { "DESPEJADO", "PARCIALMENTE NUBLADO", "NUBLADO", "NIEBLA", "LLOVIZNA", "LLUVIA", "LLUVIA INTENSA", "NIEVE", "CHUBASCOS", "CHUBASCOS DE NIEVE", "TORMENTA", "TORMENTA CON GRANIZO", "DESCONOCIDO", "NIEVE" } },
    { "sv", { "KLART", "DELVIS MOLNIGT", "MOLNIGT", "DIMMA", "DUGGREGN", "REGN", "KRAFTIGT REGN", "SNO", "REGNSKURAR", "SNOBYAR", "ASKVAEDER", "ASKVAEDER MED HAGEL", "OKAND", "SNO" } },
    { "sw", { "WAZI", "MAWINGU KIDOGO", "MAWINGU", "UKUNGU", "MANYUNYU", "MVUA", "MVUA KUBWA", "THELUJI", "MANYUNYU YA MVUA", "MANYUNYA YA THELUJI", "RADI", "RADI NA MVUA YA MAWE", "HAIJULIKANI", "THELUJI" } },
    { "tr", { "ACIK", "AZ BULUTLU", "BULUTLU", "SISLI", "CISELEME", "YAGMUR", "SIDDETLI YAGMUR", "KAR", "SAGANAK", "KAR SAGANAGI", "FIRTINA", "DOLU FIRTINASI", "BILINMIYOR", "KAR" } }
};

#define WEATHER_CODE_MAPPINGS_COUNT (sizeof(weather_code_mappings)/sizeof(weather_code_mappings[0]))

inline const char* getWeatherByCode(int code, const char* lang) {
    int descIndex = WC_UNKNOWN;

    switch(code) {
        case 0: descIndex = WC_CLEAR; break;
        case 1: case 2: descIndex = WC_PARTLY_CLOUDY; break;
        case 3: descIndex = WC_CLOUDY; break;
        case 45: case 48: descIndex = WC_FOG; break;
        case 51: case 53: case 55: descIndex = WC_DRIZZLE; break;
        case 61: case 63: descIndex = WC_RAIN; break;
        case 65: descIndex = WC_HEAVY_RAIN; break;
        case 71: case 73: case 75: descIndex = WC_SNOW; break;
        case 77: descIndex = WC_SNOW_GRAINS; break;
        case 80: case 81: case 82: descIndex = WC_SHOWERS; break;
        case 85: case 86: descIndex = WC_SNOW_SHOWERS; break;
        case 95: descIndex = WC_THUNDERSTORM; break;
        case 96: case 99: descIndex = WC_THUNDERSTORM_HAIL; break;
        default: descIndex = WC_UNKNOWN; break;
    }

    for (size_t i = 0; i < WEATHER_CODE_MAPPINGS_COUNT; i++) {
        if (strcmp(lang, weather_code_mappings[i].lang) == 0) {
            return weather_code_mappings[i].descriptions[descIndex];
        }
    }

    // Fallback to English
    return weather_code_mappings[0].descriptions[descIndex];
}

const WeatherTermsMapping weather_terms[] = {
    { "fr", {
        "CLAIR", "PARTIELLEMENT NUAGEUX", "NUAGEUX", "COUVERT", "BROUILLARD",
        "BRUINE", "PLUIE", "FORTE PLUIE", "NEIGE", "AVERSES", "AVERSES DE NEIGE",
        "ORAGE", "GRELE", "BRUME", "LEGERE", "MODEREE", "FORTE",
        "NUAGES FRAGMENTES", "NUAGES EPARS", "PEU NUAGEUX"
    }},
    { "es", {
        "DESPEJADO", "PARCIALMENTE NUBLADO", "NUBLADO", "CUBIERTO", "NIEBLA",
        "LLOVIZNA", "LLUVIA", "LLUVIA INTENSA", "NIEVE", "CHUBASCOS", "CHUBASCOS DE NIEVE",
        "TORMENTA", "GRANIZO", "NEBLINA", "LIGERA", "MODERADA", "INTENSA",
        "NUBES ROTAS", "NUBES DISPERSAS", "POCAS NUBES"
    }},
    { "de", {
        "KLAR", "TEILWEISE BEWOLKT", "WOLKIG", "BEDECKT", "NEBEL",
        "NIESELREGEN", "REGEN", "STARKREGEN", "SCHNEE", "SCHAUER", "SCHNEESCHAUER",
        "GEWITTER", "HAGEL", "DUNST", "LEICHT", "MAESSIG", "STARK",
        "AUFGELOCKERTE WOLKEN", "VEREINZELTE WOLKEN", "WENIGE WOLKEN"
    }},
    { "it", {
        "SERENO", "PARZIALMENTE NUVOLOSO", "NUVOLOSO", "COPERTO", "NEBBIA",
        "PIOVIGGINE", "PIOGGIA", "PIOGGIA FORTE", "NEVE", "ROVESCI", "ROVESCI DI NEVE",
        "TEMPORALE", "GRANDINE", "FOSCHIA", "LEGGERA", "MODERATA", "FORTE",
        "NUBI IRREGOLARI", "NUBI SPARSE", "POCHE NUVOLE"
    }},
    { "pt", {
        "LIMPO", "PARCIALMENTE NUBLADO", "NUBLADO", "ENCOBERTO", "NEVOEIRO",
        "CHUVISCO", "CHUVA", "CHUVA FORTE", "NEVE", "PANCADAS", "PANCADAS DE NEVE",
        "TROVOADA", "GRANIZO", "NEVOA", "LEVE", "MODERADA", "FORTE",
        "NUVENS QUEBRADAS", "NUVENS DISPERSAS", "POUCAS NUVENS"
    }},
    { "nl", {
        "HELDER", "GEDEELTELIJK BEWOLKT", "BEWOLKT", "OVERTROKKEN", "MIST",
        "MOTREGEN", "REGEN", "ZWARE REGEN", "SNEEUW", "BUIEN", "SNEEUWBUIEN",
        "ONWEER", "HAGEL", "NEVEL", "LICHT", "MATIG", "ZWAAR",
        "ONDERBROKEN BEWOLKING", "VERSPREIDE BEWOLKING", "WEINIG BEWOLKING"
    }}
};

#define WEATHER_TERMS_COUNT (sizeof(weather_terms)/sizeof(weather_terms[0]))

inline void translateAPIWeatherTerms(String& desc, const char* lang) {
    desc.toUpperCase();

    for (size_t i = 0; i < WEATHER_TERMS_COUNT; i++) {
        if (strcmp(lang, weather_terms[i].lang) == 0) {
            const WeatherTermsMapping& translation = weather_terms[i];

            desc.replace("CLEAR", translation.terms.clear);
            desc.replace("PARTLY CLOUDY", translation.terms.partly_cloudy);
            desc.replace("CLOUDY", translation.terms.cloudy);
            desc.replace("OVERCAST", translation.terms.overcast);
            desc.replace("FOG", translation.terms.fog);
            desc.replace("DRIZZLE", translation.terms.drizzle);
            desc.replace("RAIN", translation.terms.rain);
            desc.replace("HEAVY RAIN", translation.terms.heavy_rain);
            desc.replace("SNOW", translation.terms.snow);
            desc.replace("SHOWERS", translation.terms.showers);
            desc.replace("SNOW SHOWERS", translation.terms.snow_showers);
            desc.replace("THUNDERSTORM", translation.terms.thunderstorm);
            desc.replace("HAIL", translation.terms.hail);
            desc.replace("MIST", translation.terms.mist);
            desc.replace("LIGHT", translation.terms.light);
            desc.replace("MODERATE", translation.terms.moderate);
            desc.replace("HEAVY", translation.terms.heavy);
            desc.replace("BROKEN CLOUDS", translation.terms.broken_clouds);
            desc.replace("SCATTERED CLOUDS", translation.terms.scattered_clouds);
            desc.replace("FEW CLOUDS", translation.terms.few_clouds);
            desc.replace("CLOUDS", "");
            desc.replace("SKY", "");

            break;
        }
    }
}

#endif // WEATHER_LOOKUP_H