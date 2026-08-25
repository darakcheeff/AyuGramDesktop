// Smart search implementation for RegEx & Russian/English Morphology stemming
#include "ayu/utils/smart_search.h"

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QRegularExpression>

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
	rv.replace(perfectiveGround, QString());
	if (rv == origRv) {
		rv.replace(reflexive, QString());
		auto rvBeforeAdj = rv;
		rv.replace(adjective, QString());
		if (rv != rvBeforeAdj) {
			rv.replace(participle, QString());
		} else {
			auto rvBeforeVerb = rv;
			rv.replace(verb, QString());
			if (rv == rvBeforeVerb) {
				rv.replace(noun, QString());
			}
		}
	}

	// Step 2: 'и'
	static const QRegularExpression step2(QString::fromUtf8("и$"));
	rv.replace(step2, QString());

	// Step 3: Derivational
	static const QRegularExpression derivational(QString::fromUtf8("ость?$"));
	rv.replace(derivational, QString());

	// Step 4: Superlative, 'нн', 'ь'
	static const QRegularExpression superlative(QString::fromUtf8("(ейше|ейш)$"));
	rv.replace(superlative, QString());
	static const QRegularExpression nn(QString::fromUtf8("нн$"));
	rv.replace(nn, QString::fromUtf8("н"));
	static const QRegularExpression softSign(QString::fromUtf8("ь$"));
	rv.replace(softSign, QString());

	return head + rv;
}

// English Porter Stemmer step (plurals, ed/ing, common suffixes)
QString StemEnglish(const QString &word) {
	if (word.length() <= 2) {
		return word;
	}
	auto s = word.toLower();
	if (s.endsWith(QString::fromUtf8("sses"))) {
		s.chop(2);
	} else if (s.endsWith(QString::fromUtf8("ies"))) {
		s.chop(2);
	} else if (s.endsWith(QString::fromUtf8("ss"))) {
		// keep
	} else if (s.endsWith(QChar(u's'))) {
		s.chop(1);
	}

	if (s.endsWith(QString::fromUtf8("eed"))) {
		s.chop(1);
	} else if (s.endsWith(QString::fromUtf8("ed")) && s.length() > 4) {
		s.chop(2);
	} else if (s.endsWith(QString::fromUtf8("ing")) && s.length() > 5) {
		s.chop(3);
	} else if (s.endsWith(QString::fromUtf8("tion")) && s.length() > 5) {
		s.chop(3);
	} else if (s.endsWith(QString::fromUtf8("ment")) && s.length() > 6) {
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
	if (trimmed.isEmpty()) return false;
	if (trimmed.startsWith(QString::fromUtf8("regex:"), Qt::CaseInsensitive)) {
		return true;
	}
	if (trimmed.startsWith(QString::fromUtf8("r/")) && trimmed.endsWith(QChar(u'/')) && trimmed.length() > 3) {
		return true;
	}
	if (trimmed.startsWith(QChar(u'/')) && trimmed.endsWith(QChar(u'/')) && trimmed.length() > 2) {
		return true;
	}
	return false;
}

QString ExtractRegexPattern(const QString &query) {
	auto trimmed = query.trimmed();
	if (trimmed.startsWith(QString::fromUtf8("regex:"), Qt::CaseInsensitive)) {
		trimmed = trimmed.mid(6);
	} else if (trimmed.startsWith(QString::fromUtf8("r/")) && trimmed.endsWith(QChar(u'/'))) {
		trimmed = trimmed.mid(2, trimmed.length() - 3);
	} else if (trimmed.startsWith(QChar(u'/')) && trimmed.endsWith(QChar(u'/')) && trimmed.length() > 2) {
		trimmed = trimmed.mid(1, trimmed.length() - 2);
	}
	// If user wrote \| (escaped pipe), normalize it to | for logical OR
	trimmed.replace(QString::fromUtf8("\\|"), QString::fromUtf8("|"));
	return trimmed;
}

QStringList ExtractKeywords(const QString &query) {
	QString working = query;
	// 1. Extract quoted phrases
	static const QRegularExpression quoteRx(QString::fromUtf8("\"([^\"]+)\""));
	auto qIt = quoteRx.globalMatch(working);
	QStringList results;
	while (qIt.hasNext()) {
		results.append(qIt.next().captured(1));
	}
	working.remove(quoteRx);

	// 2. Remove exclusions (-word or !word)
	static const QRegularExpression excludeRx(QString::fromUtf8("[-!]\\S+"));
	working.remove(excludeRx);

	// 3. Handle alternations: if we have A|B, collect all alternatives as separate keywords
	// so callers can do multi-pass server queries
	static const QRegularExpression pipeRx(QString::fromUtf8("[|]"));
	if (pipeRx.match(working).hasMatch()) {
		// Extract all individual alternatives from alternation groups
		// Each token like "замена|продление" → add both "замена" and "продление"
		static const QRegularExpression tokenRx(QString::fromUtf8("[\\p{L}\\p{N}_|]{2,}"));
		auto tokIt = tokenRx.globalMatch(working);
		while (tokIt.hasNext()) {
			const auto token = tokIt.next().captured(0);
			// split on pipe
			const auto parts = token.split(QChar(u'|'), Qt::SkipEmptyParts);
			for (const auto &p : parts) {
				if (p.length() >= 2 && !results.contains(p, Qt::CaseInsensitive)) {
					results.append(p);
				}
			}
		}
		return results;
	}

	// 4. Extract alphanumeric word tokens (at least 2 chars)
	static const QRegularExpression wordRx(QString::fromUtf8("[\\p{L}\\p{N}_]{2,}"));
	auto it = wordRx.globalMatch(working);
	while (it.hasNext()) {
		const auto w = it.next().captured(0);
		if (!results.contains(w, Qt::CaseInsensitive)) {
			results.append(w);
		}
	}
	return results;
}

QString ExtractServerQuery(const QString &query) {
	const auto kw = ExtractKeywords(query);
	if (kw.isEmpty()) {
		return query.trimmed();
	}

	// Check if query contains OR alternation (A|B syntax).
	// In this case, send all alternatives to the server separated by spaces
	// so Telegram's API searches for messages containing ANY of them.
	const QString trimmed = query.trimmed();
	static const QRegularExpression pipeCheck(QString::fromUtf8("[|]"));
	if (pipeCheck.match(trimmed).hasMatch()) {
		// Return all keywords (alternatives) joined by space
		// Telegram API with multiple words performs broad OR-like search
		return kw.join(QChar(u' '));
	}

	// For AND queries (multi-word): choose the longest keyword as anchor.
	// Longer words are rarer, return fewer and more relevant server candidates.
	// Local Matches() will still require ALL words to be present.
	QString best = kw.first();
	for (const auto &k : kw) {
		if (k.length() > best.length()) {
			best = k;
		}
	}
	return best;
}

namespace {

bool MatchesSingleTerm(
		const QString &text,
		const QString &term,
		const QStringList &textStems) {
	if (term.isEmpty()) {
		return true;
	}

	// 1. Alternations: term1|term2|term3 (OR logic)
	if (term.contains(QChar(u'|'))) {
		const auto subterms = term.split(QChar(u'|'), Qt::SkipEmptyParts);
		for (const auto &sub : subterms) {
			if (MatchesSingleTerm(text, sub, textStems)) {
				return true;
			}
		}
		return false;
	}

	// 2. Wildcards (* and .*)
	if (term.contains(QChar(u'*'))) {
		if (term == QString::fromUtf8("*") || term == QString::fromUtf8(".*")) {
			return true;
		}
		auto rxPat = QRegularExpression::escape(term);
		rxPat.replace(QString::fromUtf8("\\.\\*"), QString::fromUtf8(".*"));
		rxPat.replace(QString::fromUtf8("\\*"), QString::fromUtf8(".*"));
		QRegularExpression rx(rxPat, QRegularExpression::CaseInsensitiveOption);
		if (rx.isValid()) {
			return rx.match(text).hasMatch();
		}
	}

	// 3. Exact substring check (case-insensitive)
	if (text.contains(term, Qt::CaseInsensitive)) {
		return true;
	}

	// 4. Morphological (stem-based) matching
	const auto qStem = StemWord(term);
	for (const auto &ts : textStems) {
		if (ts.contains(qStem, Qt::CaseInsensitive) || qStem.contains(ts, Qt::CaseInsensitive)) {
			return true;
		}
	}

	return false;
}

} // namespace

bool Matches(const QString &text, const QString &query) {
	if (query.isEmpty()) return true;
	if (text.isEmpty()) return false;

	// 1. Check for pure RegEx query (/pattern/ or regex:pattern)
	if (IsRegexQuery(query)) {
		const auto pattern = ExtractRegexPattern(query);
		QRegularExpression rx(pattern, QRegularExpression::CaseInsensitiveOption);
		if (rx.isValid()) {
			return rx.match(text).hasMatch();
		}
	}

	// 2. Parse advanced query tokens: exact quotes, exclusions (-word), positive terms
	QString working = query;
	QStringList exactPhrases;
	static const QRegularExpression quoteRx(QString::fromUtf8("\"([^\"]+)\""));
	auto qIt = quoteRx.globalMatch(working);
	while (qIt.hasNext()) {
		exactPhrases.append(qIt.next().captured(1));
	}
	working.remove(quoteRx);

	QStringList excludedTerms;
	QStringList positiveTerms;
	const auto tokens = working.split(QRegularExpression(QString::fromUtf8("\\s+")), Qt::SkipEmptyParts);
	for (const auto &token : tokens) {
		if ((token.startsWith(QChar(u'-')) || token.startsWith(QChar(u'!'))) && token.length() > 1) {
			excludedTerms.append(token.mid(1));
		} else {
			positiveTerms.append(token);
		}
	}

	// Prepare morphological stems for words in text
	static const QRegularExpression wordSplitter(QString::fromUtf8("[\\s,.;:!?\"'()\\[\\]{}/<>-]+"));
	const auto textWords = text.split(wordSplitter, Qt::SkipEmptyParts);
	QStringList textStems;
	textStems.reserve(textWords.size());
	for (const auto &tw : textWords) {
		textStems.append(StemWord(tw));
	}

	// 3. Exclusions check: if text contains any excluded term, reject immediately
	for (const auto &ex : excludedTerms) {
		if (ex.isEmpty()) continue;
		if (MatchesSingleTerm(text, ex, textStems)) {
			return false;
		}
	}

	// 4. Exact phrases check: all quoted phrases must be present verbatim (case-insensitive)
	for (const auto &exact : exactPhrases) {
		if (!text.contains(exact, Qt::CaseInsensitive)) {
			return false;
		}
	}

	// 5. Positive terms check: all positive terms must match (each term can be an alternation A|B or wildcard A* or word)
	for (const auto &posTerm : positiveTerms) {
		if (!MatchesSingleTerm(text, posTerm, textStems)) {
			return false;
		}
	}

	return true;
}

} // namespace SmartSearch
