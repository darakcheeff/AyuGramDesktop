// Smart search implementation for RegEx & Russian/English Morphology stemming
#include "ayu/utils/smart_search.h"

#include <QtCore/QRegularExpression>
#include <QtCore/QStringList>

namespace SmartSearch {

namespace {

bool isCyrillicVowel(QChar c) {
    static const QString vowels = QString::fromUtf8("аеиоуыэюяё");
    return vowels.contains(c);
}

// Russian Porter Stemmer implementation
QString StemRussian(const QString &word) {
    if (word.length() <= 2) {
        return word;
    }

    auto s = word.toLower();
    s.replace(QChar(u'ё'), QChar(u'е'));

    // Find RV region (after first vowel)
    int rvIndex = -1;
    for (int i = 0; i < s.length(); ++i) {
        if (isCyrillicVowel(s[i])) {
            rvIndex = i + 1;
            break;
        }
    }

    if (rvIndex == -1 || rvIndex >= s.length()) {
        return s;
    }

    auto head = s.left(rvIndex);
    auto rv = s.mid(rvIndex);

    // Step 1: Perfective gerund, Reflexive, Adjective, Participle, Verb, Noun
    static const QRegularExpression perfectiveGround(
        QString::fromUtf8("((ив|ивши|ившись|ыв|ывши|ывшись)|((?<=[ая])(в|вши|вшись)))$")
    );
    static const QRegularExpression reflexive(
        QString::fromUtf8("(с[яь])$")
    );
    static const QRegularExpression adjective(
        QString::fromUtf8("(ее|ие|ые|ое|ими|ыми|ей|ий|ый|ой|ем|им|ым|ом|его|ого|ему|ому|их|ых|ую|юю|ая|яя|ою|ею)$")
    );
    static const QRegularExpression participle(
        QString::fromUtf8("((ивш|ывш|ующ)|((?<=[ая])(ем|нн|вш|ющ|щ)))$")
    );
    static const QRegularExpression verb(
        QString::fromUtf8("((ила|ыла|ена|ейте|уйте|ите|или|ыли|ей|уй|ил|ыл|им|ым|ен|ило|ыло|ено|ят|ует|уют|ит|ыт|ены|ить|ыть|ишь)|((?<=[ая])(ла|на|ете|йте|ли|й|л|ем|н|ло|но|ет|ют|ны|ть|ешь|нно)))$")
    );
    static const QRegularExpression noun(
        QString::fromUtf8("(а|ев|ов|ие|ье|е|иями|ями|ами|еи|ии|и|ией|ей|ой|ий|й|иям|ям|ием|ем|ам|ом|о|у|ах|иях|ях|ы|ь|ию|ью|ю|ия|ья|я)$")
    );

    auto origRv = rv;
    rv.replace(perfectiveGround, "");
    if (rv == origRv) {
        rv.replace(reflexive, "");
        auto rvBeforeAdj = rv;
        rv.replace(adjective, "");
        if (rv != rvBeforeAdj) {
            rv.replace(participle, "");
        } else {
            auto rvBeforeVerb = rv;
            rv.replace(verb, "");
            if (rv == rvBeforeVerb) {
                rv.replace(noun, "");
            }
        }
    }

    // Step 2: 'и'
    rv.replace(QRegularExpression(QString::fromUtf8("и$")), "");

    // Step 3: Derivational
    static const QRegularExpression derivational(
        QString::fromUtf8("ость?$")
    );
    rv.replace(derivational, "");

    // Step 4: Superlative, 'нн', 'ь'
    static const QRegularExpression superlative(
        QString::fromUtf8("(ейше|ейш)$")
    );
    rv.replace(superlative, "");
    rv.replace(QRegularExpression(QString::fromUtf8("нн$")), QString::fromUtf8("н"));
    rv.replace(QRegularExpression(QString::fromUtf8("ь$")), "");

    return head + rv;
}

// English Porter Stemmer step (plurals, ed/ing, common suffixes)
QString StemEnglish(const QString &word) {
    if (word.length() <= 2) {
        return word;
    }
    auto s = word.toLower();
    if (s.endsWith(QStringLiteral("sses"))) {
        s.chop(2);
    } else if (s.endsWith(QStringLiteral("ies"))) {
        s.chop(2);
    } else if (s.endsWith(QStringLiteral("ss"))) {
        // keep
    } else if (s.endsWith(QChar(u's'))) {
        s.chop(1);
    }

    if (s.endsWith(QStringLiteral("eed"))) {
        s.chop(1);
    } else if (s.endsWith(QStringLiteral("ed")) && s.length() > 4) {
        s.chop(2);
    } else if (s.endsWith(QStringLiteral("ing")) && s.length() > 5) {
        s.chop(3);
    } else if (s.endsWith(QStringLiteral("tion")) && s.length() > 5) {
        s.chop(3);
    } else if (s.endsWith(QStringLiteral("ment")) && s.length() > 6) {
        s.chop(4);
    }

    return s;
}

} // namespace

QString StemWord(const QString &word) {
    if (word.isEmpty()) return QString();
    QChar first = word[0];
    if ((first >= QChar(0x0400) && first <= QChar(0x04FF)) || first == QChar(u'ё') || first == QChar(u'Ё')) {
        return StemRussian(word);
    }
    return StemEnglish(word);
}

bool IsRegexQuery(const QString &query) {
    const auto trimmed = query.trimmed();
    if (trimmed.startsWith(QStringLiteral("regex:"), Qt::CaseInsensitive)) {
        return true;
    }
    if (trimmed.startsWith(QStringLiteral("r/")) && trimmed.endsWith(QChar(u'/')) && trimmed.length() > 3) {
        return true;
    }
    if (trimmed.startsWith(QChar(u'/')) && trimmed.length() > 2) {
        int lastSlash = trimmed.lastIndexOf(QChar(u'/'));
        if (lastSlash > 0) {
            return true;
        }
    }
    return false;
}

QString ExtractRegexPattern(const QString &query) {
    auto trimmed = query.trimmed();
    if (trimmed.startsWith(QStringLiteral("regex:"), Qt::CaseInsensitive)) {
        return trimmed.mid(6);
    }
    if (trimmed.startsWith(QStringLiteral("r/")) && trimmed.endsWith(QChar(u'/'))) {
        return trimmed.mid(2, trimmed.length() - 3);
    }
    if (trimmed.startsWith(QChar(u'/'))) {
        int lastSlash = trimmed.lastIndexOf(QChar(u'/'));
        if (lastSlash > 0) {
            return trimmed.mid(1, lastSlash - 1);
        }
    }
    return trimmed;
}

bool Matches(const QString &text, const QString &query) {
    if (query.isEmpty()) return true;
    if (text.isEmpty()) return false;

    // 1. Check for RegEx query
    if (IsRegexQuery(query)) {
        QString pattern = ExtractRegexPattern(query);
        QRegularExpression rx(pattern, QRegularExpression::CaseInsensitiveOption);
        if (rx.isValid()) {
            return rx.match(text).hasMatch();
        }
    }

    // 2. Exact substring check (case-insensitive)
    if (text.contains(query, Qt::CaseInsensitive)) {
        return true;
    }

    // 3. Morphological (stem-based) matching
    static const QRegularExpression wordSplitter(QStringLiteral("[\s,.;:!?"'()\[\]{}/<>-]+"));
    const auto queryWords = query.split(wordSplitter, Qt::SkipEmptyParts);
    if (queryWords.isEmpty()) {
        return false;
    }

    const auto textWords = text.split(wordSplitter, Qt::SkipEmptyParts);
    if (textWords.isEmpty()) {
        return false;
    }

    QStringList textStems;
    textStems.reserve(textWords.size());
    for (const auto &tw : textWords) {
        textStems.append(StemWord(tw));
    }

    for (const auto &qw : queryWords) {
        const auto qStem = StemWord(qw);
        bool found = false;
        for (const auto &ts : textStems) {
            if (ts.contains(qStem, Qt::CaseInsensitive) || qStem.contains(ts, Qt::CaseInsensitive)) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }

    return true;
}

} // namespace SmartSearch
