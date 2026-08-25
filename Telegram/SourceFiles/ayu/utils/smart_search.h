// Smart search utility for AST Boolean Expressions, Morphology & RegEx
#pragma once

#include <QtCore/QString>
#include <QtCore/QStringList>

namespace SmartSearch {

// Stem a single word (supports Russian & English)
QString StemWord(const QString &word);

// Check if query is a regular expression (/pattern/ or regex:pattern)
bool IsRegexQuery(const QString &query);

// Compile regex from query string
QString ExtractRegexPattern(const QString &query);

// Extract clean server keyword query for MTProto API
QString ExtractServerQuery(const QString &query);

// Extract all positive keywords from query for multi-query candidate retrieval
QStringList ExtractKeywords(const QString &query);

// Main matching function: matches text against query with support for:
// - Boolean expressions with parentheses: (замена | продление) внж
// - Exact quoted phrases: "вид на жительство"
// - Exclusions (minus-words): -посредники or !посредники
// - Wildcards: продл* or .*
// - Alternations (OR): A | B | C
// - Full Morphological stemming (Russian & English noun/verb/adjective declensions)
// - Full RegEx: /pattern/ or regex:pattern
bool Matches(const QString &text, const QString &query);

} // namespace SmartSearch
