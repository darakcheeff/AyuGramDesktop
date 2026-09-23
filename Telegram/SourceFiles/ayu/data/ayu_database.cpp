// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/data/ayu_database.h"

#include "ayu/data/entities.h"
#include "ayu/libs/sqlite/sqlite_orm.h"
#include "base/unixtime.h"
#include <mutex>
#include <thread>
#include <condition_variable>
#include <queue>
#include <future>
#include <atomic>
#include <functional>

using namespace sqlite_orm;

namespace {

std::recursive_mutex storageMutex;

class DatabaseWriter final {
public:
	static DatabaseWriter &instance() {
		static DatabaseWriter writer;
		return writer;
	}

	void postAsync(std::function<void()> task) {
		{
			const auto lock = std::lock_guard(_mutex);
			_queue.push(std::move(task));
		}
		_cv.notify_one();
	}

	void postSync(std::function<void()> task) {
		if (std::this_thread::get_id() == _workerThreadId.load()) {
			task();
			return;
		}
		auto promise = std::make_shared<std::promise<void>>();
		auto future = promise->get_future();
		postAsync([task = std::move(task), promise]() mutable {
			try {
				task();
				promise->set_value();
			} catch (...) {
				promise->set_exception(std::current_exception());
			}
		});
		future.get();
	}

	~DatabaseWriter() {
		{
			const auto lock = std::lock_guard(_mutex);
			_stopping = true;
		}
		_cv.notify_one();
		if (_worker.joinable()) {
			_worker.join();
		}
	}

private:
	DatabaseWriter() {
		_worker = std::thread([this] { run(); });
	}

	void run() {
		_workerThreadId = std::this_thread::get_id();
		while (true) {
			std::function<void()> task;
			{
				std::unique_lock<std::mutex> lock(_mutex);
				_cv.wait(lock, [this] {
					return _stopping || !_queue.empty();
				});
				if (_stopping && _queue.empty()) {
					break;
				}
				task = std::move(_queue.front());
				_queue.pop();
			}
			if (task) {
				try {
					task();
				} catch (const std::exception &ex) {
					LOG(("AyuDatabase write error: %1").arg(ex.what()));
				} catch (...) {
					LOG(("AyuDatabase write unknown error"));
				}
			}
		}
	}

	std::mutex _mutex;
	std::condition_variable _cv;
	std::queue<std::function<void()>> _queue;
	std::atomic<bool> _stopping = false;
	std::atomic<std::thread::id> _workerThreadId;
	std::thread _worker;
};

void postWriteAsync(std::function<void()> task) {
	DatabaseWriter::instance().postAsync(std::move(task));
}

void postWriteSync(std::function<void()> task) {
	DatabaseWriter::instance().postSync(std::move(task));
}

} // namespace

auto storage = make_storage(
	"./tdata/ayudata.db",
	make_table<SchemaVersion>(
		"SchemaVersion",
		make_column("id", &SchemaVersion::id, primary_key()),
		make_column("version", &SchemaVersion::version)
	),
	make_index("idx_deleted_message_userId_dialogId_topicId_messageId",
			   column<DeletedMessage>(&DeletedMessage::userId),
			   column<DeletedMessage>(&DeletedMessage::dialogId),
			   column<DeletedMessage>(&DeletedMessage::topicId),
			   column<DeletedMessage>(&DeletedMessage::messageId)),
	make_index("idx_edited_message_userId_dialogId_messageId",
			   column<EditedMessage>(&EditedMessage::userId),
			   column<EditedMessage>(&EditedMessage::dialogId),
			   column<EditedMessage>(&EditedMessage::messageId)),
	make_index("idx_local_message_userId_dialogId_messageId",
			   column<LocalMessage>(&LocalMessage::userId),
			   column<LocalMessage>(&LocalMessage::dialogId),
			   column<LocalMessage>(&LocalMessage::messageId)),
	make_index("idx_local_message_userId_dialogId_date",
			   column<LocalMessage>(&LocalMessage::userId),
			   column<LocalMessage>(&LocalMessage::dialogId),
			   column<LocalMessage>(&LocalMessage::date)),
	make_table<DeletedMessage>(
		"DeletedMessage",
		make_column("fakeId", &DeletedMessage::fakeId, primary_key().autoincrement()),
		make_column("userId", &DeletedMessage::userId),
		make_column("dialogId", &DeletedMessage::dialogId),
		make_column("groupedId", &DeletedMessage::groupedId),
		make_column("peerId", &DeletedMessage::peerId),
		make_column("fromId", &DeletedMessage::fromId),
		make_column("topicId", &DeletedMessage::topicId),
		make_column("messageId", &DeletedMessage::messageId),
		make_column("date", &DeletedMessage::date),
		make_column("flags", &DeletedMessage::flags),
		make_column("editDate", &DeletedMessage::editDate),
		make_column("views", &DeletedMessage::views),
		make_column("fwdFlags", &DeletedMessage::fwdFlags),
		make_column("fwdFromId", &DeletedMessage::fwdFromId),
		make_column("fwdName", &DeletedMessage::fwdName),
		make_column("fwdDate", &DeletedMessage::fwdDate),
		make_column("fwdPostAuthor", &DeletedMessage::fwdPostAuthor),
		make_column("replyFlags", &DeletedMessage::replyFlags),
		make_column("replyMessageId", &DeletedMessage::replyMessageId),
		make_column("replyPeerId", &DeletedMessage::replyPeerId),
		make_column("replyTopId", &DeletedMessage::replyTopId),
		make_column("replyForumTopic", &DeletedMessage::replyForumTopic),
		make_column("replySerialized", &DeletedMessage::replySerialized),
		make_column("entityCreateDate", &DeletedMessage::entityCreateDate),
		make_column("text", &DeletedMessage::text),
		make_column("textEntities", &DeletedMessage::textEntities),
		make_column("mediaPath", &DeletedMessage::mediaPath),
		make_column("hqThumbPath", &DeletedMessage::hqThumbPath),
		make_column("documentType", &DeletedMessage::documentType),
		make_column("documentSerialized", &DeletedMessage::documentSerialized),
		make_column("thumbsSerialized", &DeletedMessage::thumbsSerialized),
		make_column("documentAttributesSerialized", &DeletedMessage::documentAttributesSerialized),
		make_column("mimeType", &DeletedMessage::mimeType)
	),
	make_table<EditedMessage>(
		"EditedMessage",
		make_column("fakeId", &EditedMessage::fakeId, primary_key().autoincrement()),
		make_column("userId", &EditedMessage::userId),
		make_column("dialogId", &EditedMessage::dialogId),
		make_column("groupedId", &EditedMessage::groupedId),
		make_column("peerId", &EditedMessage::peerId),
		make_column("fromId", &EditedMessage::fromId),
		make_column("topicId", &EditedMessage::topicId),
		make_column("messageId", &EditedMessage::messageId),
		make_column("date", &EditedMessage::date),
		make_column("flags", &EditedMessage::flags),
		make_column("editDate", &EditedMessage::editDate),
		make_column("views", &EditedMessage::views),
		make_column("fwdFlags", &EditedMessage::fwdFlags),
		make_column("fwdFromId", &EditedMessage::fwdFromId),
		make_column("fwdName", &EditedMessage::fwdName),
		make_column("fwdDate", &EditedMessage::fwdDate),
		make_column("fwdPostAuthor", &EditedMessage::fwdPostAuthor),
		make_column("replyFlags", &EditedMessage::replyFlags),
		make_column("replyMessageId", &EditedMessage::replyMessageId),
		make_column("replyPeerId", &EditedMessage::replyPeerId),
		make_column("replyTopId", &EditedMessage::replyTopId),
		make_column("replyForumTopic", &EditedMessage::replyForumTopic),
		make_column("replySerialized", &EditedMessage::replySerialized),
		make_column("entityCreateDate", &EditedMessage::entityCreateDate),
		make_column("text", &EditedMessage::text),
		make_column("textEntities", &EditedMessage::textEntities),
		make_column("mediaPath", &EditedMessage::mediaPath),
		make_column("hqThumbPath", &EditedMessage::hqThumbPath),
		make_column("documentType", &EditedMessage::documentType),
		make_column("documentSerialized", &EditedMessage::documentSerialized),
		make_column("thumbsSerialized", &EditedMessage::thumbsSerialized),
		make_column("documentAttributesSerialized", &EditedMessage::documentAttributesSerialized),
		make_column("mimeType", &EditedMessage::mimeType)
	),
	make_table<LocalMessage>(
		"LocalMessage",
		make_column("fakeId", &LocalMessage::fakeId, primary_key().autoincrement()),
		make_column("userId", &LocalMessage::userId),
		make_column("dialogId", &LocalMessage::dialogId),
		make_column("groupedId", &LocalMessage::groupedId),
		make_column("peerId", &LocalMessage::peerId),
		make_column("fromId", &LocalMessage::fromId),
		make_column("topicId", &LocalMessage::topicId),
		make_column("messageId", &LocalMessage::messageId),
		make_column("date", &LocalMessage::date),
		make_column("flags", &LocalMessage::flags),
		make_column("editDate", &LocalMessage::editDate),
		make_column("views", &LocalMessage::views),
		make_column("fwdFlags", &LocalMessage::fwdFlags),
		make_column("fwdFromId", &LocalMessage::fwdFromId),
		make_column("fwdName", &LocalMessage::fwdName),
		make_column("fwdDate", &LocalMessage::fwdDate),
		make_column("fwdPostAuthor", &LocalMessage::fwdPostAuthor),
		make_column("replyFlags", &LocalMessage::replyFlags),
		make_column("replyMessageId", &LocalMessage::replyMessageId),
		make_column("replyPeerId", &LocalMessage::replyPeerId),
		make_column("replyTopId", &LocalMessage::replyTopId),
		make_column("replyForumTopic", &LocalMessage::replyForumTopic),
		make_column("replySerialized", &LocalMessage::replySerialized),
		make_column("entityCreateDate", &LocalMessage::entityCreateDate),
		make_column("text", &LocalMessage::text),
		make_column("textEntities", &LocalMessage::textEntities),
		make_column("mediaPath", &LocalMessage::mediaPath),
		make_column("hqThumbPath", &LocalMessage::hqThumbPath),
		make_column("documentType", &LocalMessage::documentType),
		make_column("documentSerialized", &LocalMessage::documentSerialized),
		make_column("thumbsSerialized", &LocalMessage::thumbsSerialized),
		make_column("documentAttributesSerialized", &LocalMessage::documentAttributesSerialized),
		make_column("mimeType", &LocalMessage::mimeType)
	),
	make_table<DeletedDialog>(
		"DeletedDialog",
		make_column("fakeId", &DeletedDialog::fakeId, primary_key().autoincrement()),
		make_column("userId", &DeletedDialog::userId),
		make_column("dialogId", &DeletedDialog::dialogId),
		make_column("peerId", &DeletedDialog::peerId),
		make_column("folderId", &DeletedDialog::folderId),
		make_column("topMessage", &DeletedDialog::topMessage),
		make_column("lastMessageDate", &DeletedDialog::lastMessageDate),
		make_column("flags", &DeletedDialog::flags),
		make_column("entityCreateDate", &DeletedDialog::entityCreateDate)
	),
	make_table<CachedDialogs>(
		"CachedDialogs",
		make_column("fakeId", &CachedDialogs::fakeId, primary_key().autoincrement()),
		make_column("userId", &CachedDialogs::userId),
		make_column("folderId", &CachedDialogs::folderId),
		make_column("serialized", &CachedDialogs::serialized)
	),
	make_table<RegexFilter>(
		"RegexFilter",
		make_column("id", &RegexFilter::id, primary_key()),
		make_column("text", &RegexFilter::text),
		make_column("enabled", &RegexFilter::enabled),
		make_column("reversed", &RegexFilter::reversed),
		make_column("caseInsensitive", &RegexFilter::caseInsensitive),
		make_column("dialogId", &RegexFilter::dialogId)
	),
	make_table<RegexFilterGlobalExclusion>(
		"RegexFilterGlobalExclusion",
		make_column("fakeId", &RegexFilterGlobalExclusion::fakeId, primary_key().autoincrement()),
		make_column("dialogId", &RegexFilterGlobalExclusion::dialogId),
		make_column("filterId", &RegexFilterGlobalExclusion::filterId)
	),
	make_table<SpyMessageRead>(
		"SpyMessageRead",
		make_column("fakeId", &SpyMessageRead::fakeId, primary_key().autoincrement()),
		make_column("userId", &SpyMessageRead::userId),
		make_column("dialogId", &SpyMessageRead::dialogId),
		make_column("messageId", &SpyMessageRead::messageId),
		make_column("entityCreateDate", &SpyMessageRead::entityCreateDate)
	),
	make_table<SpyMessageContentsRead>(
		"SpyMessageContentsRead",
		make_column("fakeId", &SpyMessageContentsRead::fakeId, primary_key().autoincrement()),
		make_column("userId", &SpyMessageContentsRead::userId),
		make_column("dialogId", &SpyMessageContentsRead::dialogId),
		make_column("messageId", &SpyMessageContentsRead::messageId),
		make_column("entityCreateDate", &SpyMessageContentsRead::entityCreateDate)
	)
);

namespace AyuMigrations {

void migrateToV1(decltype(storage) &storage) {
	// drop RegexFilter table as we've added primary_key()
	try {
		storage.drop_table_if_exists("RegexFilter");
		LOG(("Migration to V1 successful."));
	} catch (const std::exception &ex) {
		LOG(("Migration to V1 failed: %1").arg(ex.what()));
	}
}

}

void runMigrations(decltype(storage) &storage) {
	constexpr int kLatestVersion = 1;

	const std::map<int, Fn<void(decltype(storage) &)>> migrations = {
		{1, AyuMigrations::migrateToV1},
	};

	int currentVersion = 0;
	try {
		if (auto versionRow = storage.get_pointer<SchemaVersion>(1)) {
			currentVersion = versionRow->version;
		} else {
			storage.insert(SchemaVersion{1, 0});
		}
	} catch (...) {
		LOG(("No SchemaVersion, assuming 0"));
		storage.insert(SchemaVersion{1, 0});
	}

	if (currentVersion >= kLatestVersion) {
		LOG(("Database is ok"));
		return;
	}

	LOG(("Database version: %1. Latest version: %2.").arg(currentVersion).arg(kLatestVersion));

	for (int v = currentVersion + 1; v <= kLatestVersion; ++v) {
		if (migrations.contains(v)) {
			try {
				LOG(("Migration for version: %1").arg(v));
				storage.begin_transaction();

				migrations.at(v)(storage);

				storage.update_all(set(c(&SchemaVersion::version) = v), where(c(&SchemaVersion::id) == 1));
				storage.commit();
				LOG(("Applied migration for version: %1.").arg(v));
			} catch (...) {
				storage.rollback();
				LOG(("Failed to apply migration for version: %1.").arg(v));
				AyuDatabase::moveCurrentDatabase();

				return;
			}
		}
	}
}

namespace AyuDatabase {

void moveCurrentDatabase() {
	const auto time = base::unixtime::now();

	if (QFile::exists("./tdata/ayudata.db")) {
		QFile::rename("./tdata/ayudata.db", QString("./tdata/ayudata_%1.db").arg(time));
	}

	if (QFile::exists("./tdata/ayudata.db-shm")) {
		QFile::rename("./tdata/ayudata.db-shm", QString("./tdata/ayudata_%1.db-shm").arg(time));
	}

	if (QFile::exists("./tdata/ayudata.db-wal")) {
		QFile::rename("./tdata/ayudata.db-wal", QString("./tdata/ayudata_%1.db-wal").arg(time));
	}
}

void initialize() {
	const auto lock = std::lock_guard(storageMutex);
	try {
		storage.sync_schema(true);
		try {
			storage.pragma.journal_mode(sqlite_orm::journal_mode::WAL);
			storage.pragma.synchronous(1);
		} catch (...) {
		}

		runMigrations(storage);

		storage.sync_schema(true);
	} catch (const std::exception &ex) {
		LOG(("Database initialization failed: %1").arg(ex.what()));
		moveCurrentDatabase();

		storage.sync_schema(true);
		try {
			storage.pragma.journal_mode(sqlite_orm::journal_mode::WAL);
			storage.pragma.synchronous(1);
		} catch (...) {
		}

		if (!storage.get_pointer<SchemaVersion>(1)) {
			storage.insert(SchemaVersion{1, 0});
		}
	}
	DatabaseWriter::instance();
}

void addEditedMessage(const EditedMessage &message) {
	postWriteAsync([message] {
		const auto lock = std::lock_guard(storageMutex);
		try {
			storage.begin_transaction();
			storage.insert(message);
			storage.commit();
		} catch (std::exception &ex) {
			try {
				storage.rollback();
			} catch (...) {
			}
			LOG(("Failed to save edited message for some reason: %1").arg(ex.what()));
		}
	});
}

std::vector<EditedMessage> getEditedMessages(ID userId, ID dialogId, ID messageId, ID minId, ID maxId, int totalLimit) {
	const auto lock = std::lock_guard(storageMutex);
	return storage.get_all<EditedMessage>(
		where(
			column<EditedMessage>(&EditedMessage::userId) == userId and
			column<EditedMessage>(&EditedMessage::dialogId) == dialogId and
			column<EditedMessage>(&EditedMessage::messageId) == messageId and
			(column<EditedMessage>(&EditedMessage::fakeId) > minId or minId == 0) and
			(column<EditedMessage>(&EditedMessage::fakeId) < maxId or maxId == 0)
		),
		order_by(column<EditedMessage>(&EditedMessage::fakeId)).desc(),
		limit(totalLimit)
	);
}

bool hasRevisions(ID userId, ID dialogId, ID messageId) {
	const auto lock = std::lock_guard(storageMutex);
	try {
		return !storage.select(
			columns(column<EditedMessage>(&EditedMessage::messageId)),
			where(
				column<EditedMessage>(&EditedMessage::userId) == userId and
				column<EditedMessage>(&EditedMessage::dialogId) == dialogId and
				column<EditedMessage>(&EditedMessage::messageId) == messageId
			),
			limit(1)
		).empty();
	} catch (std::exception &ex) {
		LOG(("Failed to check if message has revisions: %1").arg(ex.what()));
		return false;
	}
}

void addDeletedMessage(const DeletedMessage &message) {
	postWriteAsync([message] {
		const auto lock = std::lock_guard(storageMutex);
		try {
			storage.begin_transaction();
			storage.insert(message);
			storage.commit();
		} catch (std::exception &ex) {
			try {
				storage.rollback();
			} catch (...) {
			}
			LOG(("Failed to save deleted message for some reason: %1").arg(ex.what()));
		}
	});
}

std::vector<DeletedMessage> getDeletedMessages(ID userId, ID dialogId, ID topicId, ID minId, ID maxId, int totalLimit, const std::string &searchQuery) {
	const auto lock = std::lock_guard(storageMutex);
	if (searchQuery.empty()) {
		return storage.get_all<DeletedMessage>(
			where(
				column<DeletedMessage>(&DeletedMessage::userId) == userId and
				column<DeletedMessage>(&DeletedMessage::dialogId) == dialogId and
				(column<DeletedMessage>(&DeletedMessage::topicId) == topicId or topicId == 0) and
				(column<DeletedMessage>(&DeletedMessage::messageId) > minId or minId == 0) and
				(column<DeletedMessage>(&DeletedMessage::messageId) < maxId or maxId == 0)
			),
			order_by(column<DeletedMessage>(&DeletedMessage::messageId)).desc(),
			limit(totalLimit)
		);
	}

	std::string escaped;
	escaped.reserve(searchQuery.size());
	for (const auto c : searchQuery) {
		if (c == '%' || c == '_' || c == '\\') {
			escaped += '\\';
		}
		escaped += c;
	}
	const auto pattern = "%" + escaped + "%";
	return storage.get_all<DeletedMessage>(
		where(
			column<DeletedMessage>(&DeletedMessage::userId) == userId and
			column<DeletedMessage>(&DeletedMessage::dialogId) == dialogId and
			(column<DeletedMessage>(&DeletedMessage::topicId) == topicId or topicId == 0) and
			(column<DeletedMessage>(&DeletedMessage::messageId) > minId or minId == 0) and
			(column<DeletedMessage>(&DeletedMessage::messageId) < maxId or maxId == 0) and
			like(column<DeletedMessage>(&DeletedMessage::text), pattern, "\\")
		),
		order_by(column<DeletedMessage>(&DeletedMessage::messageId)).desc(),
		limit(totalLimit)
	);
}

bool hasDeletedMessages(ID userId, ID dialogId, ID topicId) {
	const auto lock = std::lock_guard(storageMutex);
	try {
		return !storage.select(
			columns(column<DeletedMessage>(&DeletedMessage::dialogId)),
			where(
				column<DeletedMessage>(&DeletedMessage::userId) == userId and
				column<DeletedMessage>(&DeletedMessage::dialogId) == dialogId and
				(column<DeletedMessage>(&DeletedMessage::topicId) == topicId or topicId == 0)
			),
			limit(1)
		).empty();
	} catch (std::exception &ex) {
		LOG(("Failed to check if dialog has deleted message: %1").arg(ex.what()));
		return false;
	}
}

void removeDeletedMessage(ID userId, ID dialogId, ID messageId) {
	postWriteAsync([userId, dialogId, messageId] {
		const auto lock = std::lock_guard(storageMutex);
		try {
			storage.remove_all<DeletedMessage>(
				where(
					column<DeletedMessage>(&DeletedMessage::userId) == userId and
					column<DeletedMessage>(&DeletedMessage::dialogId) == dialogId and
					column<DeletedMessage>(&DeletedMessage::messageId) == messageId
				)
			);
		} catch (std::exception &ex) {
			LOG(("Failed to remove deleted message: %1").arg(ex.what()));
		}
	});
}

void clearDeletedMessages(ID userId, ID dialogId, ID topicId) {
	postWriteAsync([userId, dialogId, topicId] {
		const auto lock = std::lock_guard(storageMutex);
		try {
			storage.remove_all<DeletedMessage>(
				where(
					column<DeletedMessage>(&DeletedMessage::userId) == userId and
					column<DeletedMessage>(&DeletedMessage::dialogId) == dialogId and
					(column<DeletedMessage>(&DeletedMessage::topicId) == topicId or topicId == 0)
				)
			);
		} catch (std::exception &) {
		}
	});
}

void addLocalMessage(const LocalMessage &message) {
	postWriteAsync([message] {
		const auto lock = std::lock_guard(storageMutex);
		try {
			storage.remove_all<LocalMessage>(
				where(
					column<LocalMessage>(&LocalMessage::userId) == message.userId and
					column<LocalMessage>(&LocalMessage::dialogId) == message.dialogId and
					column<LocalMessage>(&LocalMessage::messageId) == message.messageId
				)
			);
			storage.insert(message);
		} catch (std::exception &ex) {
			LOG(("Failed to save local message: %1").arg(ex.what()));
		}
	});
}

std::vector<LocalMessage> getLocalMessages(ID userId, ID dialogId, ID topicId, ID minId, ID maxId, int totalLimit) {
	const auto lock = std::lock_guard(storageMutex);
	try {
		return storage.get_all<LocalMessage>(
			where(
				column<LocalMessage>(&LocalMessage::userId) == userId and
				column<LocalMessage>(&LocalMessage::dialogId) == dialogId and
				(column<LocalMessage>(&LocalMessage::topicId) == topicId or topicId == 0) and
				(column<LocalMessage>(&LocalMessage::messageId) > minId or minId == 0) and
				(column<LocalMessage>(&LocalMessage::messageId) < maxId or maxId == 0)
			),
			order_by(column<LocalMessage>(&LocalMessage::messageId)).desc(),
			limit(totalLimit)
		);
	} catch (std::exception &ex) {
		LOG(("Failed to get local messages: %1").arg(ex.what()));
		return {};
	}
}

std::vector<LocalMessage> searchLocalMessages(ID userId, const std::string &searchQuery, ID dialogId, ID fromId, int totalLimit) {
	const auto lock = std::lock_guard(storageMutex);
	try {
		const auto hasQuery = !searchQuery.empty();
		std::string escaped;
		if (hasQuery) {
			escaped.reserve(searchQuery.size());
			for (const auto c : searchQuery) {
				if (c == '%' || c == '_' || c == '\\') {
					escaped += '\\';
				}
				escaped += c;
			}
		}
		const auto pattern = "%" + escaped + "%";

		if (dialogId != 0) {
			return storage.get_all<LocalMessage>(
				where(
					column<LocalMessage>(&LocalMessage::userId) == userId and
					column<LocalMessage>(&LocalMessage::dialogId) == dialogId and
					(column<LocalMessage>(&LocalMessage::fromId) == fromId or fromId == 0) and
					(like(column<LocalMessage>(&LocalMessage::text), pattern, "\\") or not hasQuery)
				),
				order_by(column<LocalMessage>(&LocalMessage::date)).desc(),
				limit(totalLimit)
			);
		} else {
			return storage.get_all<LocalMessage>(
				where(
					column<LocalMessage>(&LocalMessage::userId) == userId and
					(column<LocalMessage>(&LocalMessage::fromId) == fromId or fromId == 0) and
					(like(column<LocalMessage>(&LocalMessage::text), pattern, "\\") or not hasQuery)
				),
				order_by(column<LocalMessage>(&LocalMessage::date)).desc(),
				limit(totalLimit)
			);
		}
	} catch (std::exception &ex) {
		LOG(("Failed to search local messages: %1").arg(ex.what()));
		return {};
	}
}

void clearLocalMessages(int olderThanSecs) {
	postWriteAsync([olderThanSecs] {
		const auto lock = std::lock_guard(storageMutex);
		try {
			if (olderThanSecs > 0) {
				const auto cutoff = base::unixtime::now() - olderThanSecs;
				storage.remove_all<LocalMessage>(
					where(column<LocalMessage>(&LocalMessage::date) < cutoff)
				);
			} else {
				storage.remove_all<LocalMessage>();
			}
		} catch (std::exception &ex) {
			LOG(("Failed to clear local messages: %1").arg(ex.what()));
		}
	});
}

void saveCachedDialogs(ID userId, int folderId, const std::vector<char> &serialized) {
	postWriteAsync([userId, folderId, serialized] {
		const auto lock = std::lock_guard(storageMutex);
		try {
			storage.remove_all<CachedDialogs>(
				where(
					column<CachedDialogs>(&CachedDialogs::userId) == userId and
					column<CachedDialogs>(&CachedDialogs::folderId) == folderId
				)
			);
			CachedDialogs row;
			row.userId = userId;
			row.folderId = folderId;
			row.serialized = serialized;
			storage.insert(row);
		} catch (const std::exception &ex) {
			LOG(("Failed to save cached dialogs: %1").arg(ex.what()));
		}
	});
}

std::vector<char> getCachedDialogs(ID userId, int folderId) {
	const auto lock = std::lock_guard(storageMutex);
	try {
		auto rows = storage.get_all<CachedDialogs>(
			where(
				column<CachedDialogs>(&CachedDialogs::userId) == userId and
				column<CachedDialogs>(&CachedDialogs::folderId) == folderId
			),
			limit(1)
		);
		if (!rows.empty()) {
			return std::move(rows.front().serialized);
		}
	} catch (const std::exception &ex) {
		LOG(("Failed to get cached dialogs: %1").arg(ex.what()));
	}
	return {};
}

template<typename T>
std::vector<T> getAllT() {
	const auto lock = std::lock_guard(storageMutex);
	try {
		return storage.get_all<T>();
	} catch (std::exception &ex) {
		LOG(("Failed to get all: %1").arg(ex.what()));
		return {};
	}
}

std::vector<RegexFilter> getAllRegexFilters() {
	return getAllT<RegexFilter>();
}

std::vector<RegexFilterGlobalExclusion> getAllFiltersExclusions() {
	return getAllT<RegexFilterGlobalExclusion>();
}

std::vector<RegexFilter> getExcludedByDialogId(ID dialogId) {
	const auto lock = std::lock_guard(storageMutex);
	try {
		return storage.get_all<RegexFilter>(
			where(in(&RegexFilter::id,
					 storage.select(columns(&RegexFilterGlobalExclusion::filterId),
									where(is_equal(&RegexFilterGlobalExclusion::dialogId, dialogId))
					 )
			))
		);
	} catch (std::exception &ex) {
		LOG(("Failed to get excluded by dialog id: %1").arg(ex.what()));
		return {};
	}
}

int getCount() {
	const auto lock = std::lock_guard(storageMutex);
	try {
		return storage.count<RegexFilter>();
	} catch (std::exception &ex) {
		LOG(("Failed to get count: %1").arg(ex.what()));
		return 0;
	}
}

RegexFilter getById(std::vector<char> id) {
	const auto lock = std::lock_guard(storageMutex);
	try {
		return storage.get<RegexFilter>(
			where(column<RegexFilter>(&RegexFilter::id) == std::move(id))
		);
	} catch (std::exception &ex) {
		LOG(("Failed to get filters by id: %1").arg(ex.what()));
		return {};
	}
}

std::vector<RegexFilter> getShared() {
	const auto lock = std::lock_guard(storageMutex);
	try {
		return storage.get_all<RegexFilter>(
			where(is_null(column<RegexFilter>(&RegexFilter::dialogId)))
		);
	} catch (std::exception &ex) {
		LOG(("Failed to get shared filters: %1").arg(ex.what()));
		return {};
	}
}

std::vector<RegexFilter> getByDialogId(ID dialogId) {
	const auto lock = std::lock_guard(storageMutex);
	try {
		return storage.get_all<RegexFilter>(
			where(column<RegexFilter>(&RegexFilter::dialogId) == dialogId)
		);
	} catch (std::exception &ex) {
		LOG(("Failed to get filters by dialog id: %1").arg(ex.what()));
		return {};
	}
}

void addRegexFilter(const RegexFilter &filter) {
	postWriteSync([filter] {
		const auto lock = std::lock_guard(storageMutex);
		try {
			storage.begin_transaction();
			storage.replace(filter); // we're using replace as we set std::vector<char> as primary key
			storage.commit();
		} catch (std::exception &ex) {
			try {
				storage.rollback();
			} catch (...) {
			}
			LOG(("Failed to save regex filter for some reason: %1").arg(ex.what()));
		}
	});
}

void addRegexExclusion(const RegexFilterGlobalExclusion &exclusion) {
	postWriteSync([exclusion] {
		const auto lock = std::lock_guard(storageMutex);
		try {
			storage.begin_transaction();
			storage.insert(exclusion);
			storage.commit();
		} catch (std::exception &ex) {
			try {
				storage.rollback();
			} catch (...) {
			}
			LOG(("Failed to save regex filter exclusion for some reason: %1").arg(ex.what()));
		}
	});
}

void updateRegexFilter(const RegexFilter &filter) {
	postWriteSync([filter] {
		const auto lock = std::lock_guard(storageMutex);
		try {
			storage.update_all(
				set(
					c(&RegexFilter::text) = filter.text,
					c(&RegexFilter::enabled) = filter.enabled,
					c(&RegexFilter::reversed) = filter.reversed,
					c(&RegexFilter::caseInsensitive) = filter.caseInsensitive,
					c(&RegexFilter::dialogId) = filter.dialogId
				),
				where(c(&RegexFilter::id) == filter.id)
			);
		} catch (std::exception &ex) {
			LOG(("Failed to update regex filter for some reason: %1").arg(ex.what()));
		}
	});
}

void deleteFilter(const std::vector<char> &id) {
	postWriteSync([id] {
		const auto lock = std::lock_guard(storageMutex);
		try {
			storage.remove_all<RegexFilter>(
				where(column<RegexFilter>(&RegexFilter::id) == id)
			);
		} catch (std::exception &ex) {
			LOG(("Failed to delete regex filter for some reason: %1").arg(ex.what()));
		}
	});
}

void deleteExclusionsByFilterId(const std::vector<char> &id) {
	postWriteSync([id] {
		const auto lock = std::lock_guard(storageMutex);
		try {
			storage.remove_all<RegexFilterGlobalExclusion>(
				where(column<RegexFilterGlobalExclusion>(&RegexFilterGlobalExclusion::filterId) == id)
			);
		} catch (std::exception &ex) {
			LOG(("Failed to delete regex filter exclusion by filter id for some reason: %1").arg(ex.what()));
		}
	});
}

void deleteExclusion(ID dialogId, std::vector<char> filterId) {
	postWriteSync([dialogId, filterId = std::move(filterId)] {
		const auto lock = std::lock_guard(storageMutex);
		try {
			storage.remove_all<RegexFilterGlobalExclusion>(
				where(column<RegexFilterGlobalExclusion>(&RegexFilterGlobalExclusion::filterId) == filterId and
					column<RegexFilterGlobalExclusion>(&RegexFilterGlobalExclusion::dialogId) == dialogId
				)
			);
		} catch (std::exception &ex) {
			LOG(("Failed to delete regex filter exclusion for some reason: %1").arg(ex.what()));
		}
	});
}

void deleteAllFilters() {
	postWriteSync([] {
		const auto lock = std::lock_guard(storageMutex);
		try {
			storage.remove_all<RegexFilter>();
		} catch (std::exception &ex) {
			LOG(("Failed to delete all regex filter for some reason: %1").arg(ex.what()));
		}
	});
}

void deleteAllExclusions() {
	postWriteSync([] {
		const auto lock = std::lock_guard(storageMutex);
		try {
			storage.remove_all<RegexFilterGlobalExclusion>();
		} catch (std::exception &ex) {
			LOG(("Failed to delete all regex filter exclusions for some reason: %1").arg(ex.what()));
		}
	});
}

bool hasFilters() {
	const auto lock = std::lock_guard(storageMutex);
	try {
		return !storage.select(
			columns(column<RegexFilter>(&RegexFilter::id)),
			limit(1)
		).empty();
	} catch (std::exception &ex) {
		LOG(("Failed to check if there's any filters: %1").arg(ex.what()));
		return false;
	}
}

bool hasPerDialogFilters() {
	const auto lock = std::lock_guard(storageMutex);
	try {
		return
			!storage.select(
				columns(column<RegexFilter>(&RegexFilter::id)),
				where(is_not_null(column<RegexFilter>(&RegexFilter::dialogId))),
				limit(1)
			).empty() ||
			!storage.select(
				columns(column<RegexFilterGlobalExclusion>(&RegexFilterGlobalExclusion::fakeId)),
				limit(1)
			).empty();
	} catch (std::exception &ex) {
		LOG(("Failed to check if there's any filters: %1").arg(ex.what()));
		return false;
	}
}

}
