// Smart search utility for RegEx & Russian/English Morphology stemming
#pragma once

#include <QString>
#include <QStringList>

namespace SmartSearch {

// Stem a single word (supports Russian & English)
QString StemWord(const QString &word);

// Check if query is a regular expression (/pattern/ or regex:pattern)
bool IsRegexQuery(const QString &query);

// Compile regex from query string
QString ExtractRegexPattern(const QString &query);

// Main matching function: matches text against query using regex, morphology stemming or exact substring
bool Matches(const QString &text, const QString &query);

} // namespace SmartSearch
