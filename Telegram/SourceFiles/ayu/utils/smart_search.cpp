// Smart search implementation: AST Boolean Expressions, Morphology & RegEx
#include "ayu/utils/smart_search.h"

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QRegularExpression>
#include <memory>
#include <vector>

namespace SmartSearch {

namespace {

bool isCyrillicVowel(QChar c) {
	static const QString vowels = QString::fromUtf8("аеиоуыэюяё");
	return vowels.contains(c);
}

// Improved Russian Porter Stemmer with noun-declension priority
QString StemRussian(const QString &word) {
	if (word.length() <= 2) {
		return word.toLower();
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

	// Step 1: Perfective gerund, Reflexive, Adjective, Participle, Noun, Verb
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
	static const QRegularExpression noun(
		QString::fromUtf8("(а|ев|ов|ие|ье|е|иями|ями|ами|еи|ии|и|ией|ей|ой|ий|й|иям|ям|ием|ем|ам|ом|о|у|ах|иях|ях|ы|ь|ию|ью|ю|ия|ья|я)$")
	);
	static const QRegularExpression verb(
		QString::fromUtf8("((ила|ыла|ейте|уйте|ите|или|ыли|ей|уй|ил|ыл|им|ым|ило|ыло|ено|ят|ует|уют|ит|ыт|ить|ыть|ишь)|((?<=[ая])(ла|на|ете|йте|ли|й|л|ем|н|ло|но|ет|ют|ны|ть|ешь|нно)))$")
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
			auto rvBeforeNoun = rv;
			rv.replace(noun, QString());
			if (rv == rvBeforeNoun) {
				rv.replace(verb, QString());
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

// English Porter Stemmer step
QString StemEnglish(const QString &word) {
	if (word.length() <= 2) {
		return word.toLower();
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
	trimmed.replace(QString::fromUtf8("\\|"), QString::fromUtf8("|"));
	return trimmed;
}

QStringList ExtractKeywords(const QString &query) {
	QString working = query;
	QStringList results;

	// 1. Extract exact quoted phrases: "exact phrase"
	static const QRegularExpression quoteRx(QString::fromUtf8(""([^"]+)""));
	auto qIt = quoteRx.globalMatch(working);
	while (qIt.hasNext()) {
		const auto phrase = qIt.next().captured(1).trimmed();
		if (!phrase.isEmpty() && !results.contains(phrase, Qt::CaseInsensitive)) {
			results.append(phrase);
		}
	}
	working.remove(quoteRx);

	// 2. Remove exclusions (-word or !word)
	static const QRegularExpression excludeRx(QString::fromUtf8("[-!]\S+"));
	working.remove(excludeRx);

	// 3. Remove grouping parens and operator symbols
	working.replace(QChar(u'('), QChar(u' '));
	working.replace(QChar(u')'), QChar(u' '));
	working.replace(QChar(u'|'), QChar(u' '));

	// 4. Extract positive words (at least 2 chars)
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
	// Return the longest/most specific keyword as anchor
	QString best = kw.first();
	for (const auto &k : kw) {
		if (k.length() > best.length()) {
			best = k;
		}
	}
	return best;
}

namespace {

// ==========================================
// AST Expression Engine
// ==========================================

struct EvalContext {
	QString text;
	QStringList words;
	QStringList stems;
};

class AstNode {
public:
	virtual ~AstNode() = default;
	[[nodiscard]] virtual bool evaluate(const EvalContext &ctx) const = 0;
};

class WordNode final : public AstNode {
public:
	WordNode(QString word, bool isWildcard)
	: _word(std::move(word))
	, _isWildcard(isWildcard)
	, _stem(_isWildcard ? _word : StemWord(_word)) {
	}

	bool evaluate(const EvalContext &ctx) const override {
		if (_isWildcard) {
			for (const auto &w : ctx.words) {
				if (w.startsWith(_stem, Qt::CaseInsensitive)) {
					return true;
				}
			}
			return false;
		}

		// Short words (<= 3 chars, e.g. "внж", "рвп", "кз", "тоо")
		// require exact full-word match.
		if (_word.length() <= 3) {
			for (const auto &w : ctx.words) {
				if (w.compare(_word, Qt::CaseInsensitive) == 0) {
					return true;
				}
			}
			return false;
		}

		// Standard words: match either exact word or stem equality
		for (int i = 0; i < ctx.words.size(); ++i) {
			if (ctx.words[i].compare(_word, Qt::CaseInsensitive) == 0) {
				return true;
			}
			if (i < ctx.stems.size() && !ctx.stems[i].isEmpty()) {
				if (ctx.stems[i].compare(_stem, Qt::CaseInsensitive) == 0) {
					return true;
				}
			}
		}
		return false;
	}

private:
	QString _word;
	bool _isWildcard = false;
	QString _stem;
};

class PhraseNode final : public AstNode {
public:
	explicit PhraseNode(QString phrase)
	: _phrase(std::move(phrase)) {
	}

	bool evaluate(const EvalContext &ctx) const override {
		return ctx.text.contains(_phrase, Qt::CaseInsensitive);
	}

private:
	QString _phrase;
};

class NotNode final : public AstNode {
public:
	explicit NotNode(std::unique_ptr<AstNode> child)
	: _child(std::move(child)) {
	}

	bool evaluate(const EvalContext &ctx) const override {
		return !_child->evaluate(ctx);
	}

private:
	std::unique_ptr<AstNode> _child;
};

class AndNode final : public AstNode {
public:
	explicit AndNode(std::vector<std::unique_ptr<AstNode>> children)
	: _children(std::move(children)) {
	}

	bool evaluate(const EvalContext &ctx) const override {
		for (const auto &child : _children) {
			if (!child->evaluate(ctx)) {
				return false;
			}
		}
		return true;
	}

private:
	std::vector<std::unique_ptr<AstNode>> _children;
};

class OrNode final : public AstNode {
public:
	explicit OrNode(std::vector<std::unique_ptr<AstNode>> children)
	: _children(std::move(children)) {
	}

	bool evaluate(const EvalContext &ctx) const override {
		for (const auto &child : _children) {
			if (child->evaluate(ctx)) {
				return true;
			}
		}
		return false;
	}

private:
	std::vector<std::unique_ptr<AstNode>> _children;
};

// ==========================================
// Lexer & Recursive-Descent Parser
// ==========================================

enum class TokenType {
	Word,
	Phrase,
	NotWord,
	NotPhrase,
	Pipe,       // | or OR
	OpenParen,  // (
	CloseParen, // )
	EndOfQuery,
};

struct Token {
	TokenType type = TokenType::EndOfQuery;
	QString text;
	bool isWildcard = false;
};

class Lexer {
public:
	explicit Lexer(const QString &query) : _q(query), _len(query.length()) {}

	Token next() {
		skipSpaces();
		if (_pos >= _len) {
			return { TokenType::EndOfQuery, QString() };
		}

		const QChar c = _q[_pos];
		if (c == QChar(u'(')) {
			++_pos;
			return { TokenType::OpenParen, QString(u'(') };
		}
		if (c == QChar(u')')) {
			++_pos;
			return { TokenType::CloseParen, QString(u')') };
		}
		if (c == QChar(u'|')) {
			++_pos;
			return { TokenType::Pipe, QString(u'|') };
		}

		// Exact phrase: "phrase"
		if (c == QChar(u'"')) {
			++_pos;
			const int start = _pos;
			while (_pos < _len && _q[_pos] != QChar(u'"')) {
				++_pos;
			}
			const auto phrase = _q.mid(start, _pos - start);
			if (_pos < _len && _q[_pos] == QChar(u'"')) {
				++_pos;
			}
			return { TokenType::Phrase, phrase };
		}

		// Negative phrase or word: -"phrase" or -word or !word
		if ((c == QChar(u'-') || c == QChar(u'!')) && _pos + 1 < _len) {
			++_pos;
			if (_q[_pos] == QChar(u'"')) {
				++_pos;
				const int start = _pos;
				while (_pos < _len && _q[_pos] != QChar(u'"')) {
					++_pos;
				}
				const auto phrase = _q.mid(start, _pos - start);
				if (_pos < _len && _q[_pos] == QChar(u'"')) {
					++_pos;
				}
				return { TokenType::NotPhrase, phrase };
			}
			const int start = _pos;
			while (_pos < _len && !_q[_pos].isSpace() && _q[_pos] != QChar(u'(') && _q[_pos] != QChar(u')') && _q[_pos] != QChar(u'|')) {
				++_pos;
			}
			const auto word = _q.mid(start, _pos - start);
			return { TokenType::NotWord, word };
		}

		// Normal word or wildcard
		const int start = _pos;
		while (_pos < _len && !_q[_pos].isSpace() && _q[_pos] != QChar(u'(') && _q[_pos] != QChar(u')') && _q[_pos] != QChar(u'|') && _q[_pos] != QChar(u'"')) {
			++_pos;
		}
		auto raw = _q.mid(start, _pos - start);
		if (raw.compare(QString::fromUtf8("OR"), Qt::CaseInsensitive) == 0) {
			return { TokenType::Pipe, raw };
		}
		bool isWildcard = false;
		if (raw.endsWith(QChar(u'*'))) {
			isWildcard = true;
			raw.chop(1);
		}
		return { TokenType::Word, raw, isWildcard };
	}

private:
	void skipSpaces() {
		while (_pos < _len && _q[_pos].isSpace()) {
			++_pos;
		}
	}

	QString _q;
	int _len = 0;
	int _pos = 0;
};

class Parser {
public:
	explicit Parser(const QString &query) : _lexer(query) {
		advance();
	}

	std::unique_ptr<AstNode> parse() {
		auto node = parseOrExpr();
		return node;
	}

private:
	void advance() {
		_curr = _lexer.next();
	}

	// OrExpr := AndExpr ( ("|" | "OR") AndExpr )*
	std::unique_ptr<AstNode> parseOrExpr() {
		auto left = parseAndExpr();
		if (!left) return nullptr;

		std::vector<std::unique_ptr<AstNode>> terms;
		terms.push_back(std::move(left));

		while (_curr.type == TokenType::Pipe) {
			advance(); // skip |
			auto next = parseAndExpr();
			if (next) {
				terms.push_back(std::move(next));
			}
		}

		if (terms.size() == 1) {
			return std::move(terms[0]);
		}
		return std::make_unique<OrNode>(std::move(terms));
	}

	// AndExpr := PrimaryExpr+
	std::unique_ptr<AstNode> parseAndExpr() {
		std::vector<std::unique_ptr<AstNode>> terms;

		while (_curr.type != TokenType::EndOfQuery
			&& _curr.type != TokenType::CloseParen
			&& _curr.type != TokenType::Pipe) {
			auto term = parsePrimaryExpr();
			if (term) {
				terms.push_back(std::move(term));
			}
		}

		if (terms.empty()) {
			return nullptr;
		}
		if (terms.size() == 1) {
			return std::move(terms[0]);
		}
		return std::make_unique<AndNode>(std::move(terms));
	}

	// PrimaryExpr := Word | Phrase | NotWord | NotPhrase | "(" OrExpr ")"
	std::unique_ptr<AstNode> parsePrimaryExpr() {
		if (_curr.type == TokenType::OpenParen) {
			advance(); // skip (
			auto expr = parseOrExpr();
			if (_curr.type == TokenType::CloseParen) {
				advance(); // skip )
			}
			return expr;
		}
		if (_curr.type == TokenType::Word) {
			auto node = std::make_unique<WordNode>(_curr.text, _curr.isWildcard);
			advance();
			return node;
		}
		if (_curr.type == TokenType::Phrase) {
			auto node = std::make_unique<PhraseNode>(_curr.text);
			advance();
			return node;
		}
		if (_curr.type == TokenType::NotWord) {
			auto inner = std::make_unique<WordNode>(_curr.text, false);
			auto node = std::make_unique<NotNode>(std::move(inner));
			advance();
			return node;
		}
		if (_curr.type == TokenType::NotPhrase) {
			auto inner = std::make_unique<PhraseNode>(_curr.text);
			auto node = std::make_unique<NotNode>(std::move(inner));
			advance();
			return node;
		}
		advance();
		return nullptr;
	}

	Lexer _lexer;
	Token _curr;
};

} // namespace

bool Matches(const QString &text, const QString &query) {
	const auto trimmedQuery = query.trimmed();
	if (trimmedQuery.isEmpty()) return true;
	if (text.isEmpty()) return false;

	// 1. Check for pure RegEx query (/pattern/ or regex:pattern)
	if (IsRegexQuery(trimmedQuery)) {
		const auto pattern = ExtractRegexPattern(trimmedQuery);
		QRegularExpression rx(pattern, QRegularExpression::CaseInsensitiveOption);
		if (rx.isValid()) {
			return rx.match(text).hasMatch();
		}
	}

	// 2. Tokenize text into words & morphological stems
	static const QRegularExpression wordSplitter(QString::fromUtf8("[\\s,.;:!?"\x27()\\[\\]{}/<>-]+"));
	const auto rawWords = text.split(wordSplitter, Qt::SkipEmptyParts);
	QStringList words;
	QStringList stems;
	words.reserve(rawWords.size());
	stems.reserve(rawWords.size());

	for (const auto &w : rawWords) {
		words.append(w);
		stems.append(StemWord(w));
	}

	const EvalContext ctx{
		.text = text,
		.words = words,
		.stems = stems,
	};

	// 3. Parse AST and evaluate
	Parser parser(trimmedQuery);
	const auto ast = parser.parse();
	if (!ast) {
		return true;
	}
	return ast->evaluate(ctx);
}

} // namespace SmartSearch
