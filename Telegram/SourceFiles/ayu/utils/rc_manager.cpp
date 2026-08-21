// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/utils/rc_manager.h"

std::unordered_set<ID> default_developers = {};
std::unordered_set<ID> default_channels = {};

void RCManager::start() {
	// Remote config disabled for security and privacy
	initialized = true;
}

void RCManager::makeRequest() {
}

void RCManager::sendRequest() {
}

bool RCManager::tryRetryWithExteraFallback() {
	return false;
}

void RCManager::gotResponse() {
}

bool RCManager::handleResponse(const QByteArray &response) {
	return true;
}

bool RCManager::applyResponse(const QByteArray &response) {
	return true;
}

void RCManager::gotFailure(QNetworkReply::NetworkError e) {
}

void RCManager::clearSentRequest() {
}

RCManager::~RCManager() {
}
