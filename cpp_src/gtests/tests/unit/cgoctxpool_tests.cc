#include "cgoctxpool_api.h"

#include <future>

using std::unique_ptr;
using reindexer::CancelType;

static const size_t kCtxPoolSize = 4096;

TEST_F(CGOCtxPoolApi, SingleThread) {
	static const size_t kFirstIterCount = kCtxPoolSize + 1;
	static const size_t kSecondIterCount = kCtxPoolSize * 3 / 2 + 1;
	static const size_t kThirdIterCount = kCtxPoolSize * 5 / 2 + 1;

	auto pool = createCtxPool(kCtxPoolSize);
	vector<IRdxCancelContext*> ctxPtrs(kCtxPoolSize);

	EXPECT_TRUE(getAndValidateCtx(0, *pool) == nullptr);

	for (uint64_t i = 1; i < kFirstIterCount; ++i) {
		ctxPtrs[i - 1] = getAndValidateCtx(i, *pool);
		ASSERT_TRUE(ctxPtrs[i - 1] != nullptr);
	}

	// Trying to get the same contexts
	for (uint64_t i = 1; i < kFirstIterCount; ++i) {
		ASSERT_TRUE(getAndValidateCtx(i, *pool) == nullptr);
	}

	// Get few more
	for (uint64_t i = kFirstIterCount; i < kSecondIterCount; ++i) {
		ASSERT_TRUE(getAndValidateCtx(i, *pool) != nullptr);
	}

	auto& contexts = pool->contexts();
	ASSERT_EQ(contexts.size(), kCtxPoolSize);
	for (size_t i = 0; i < kCtxPoolSize; ++i) {
		EXPECT_TRUE(ContextsPool<CGORdxContext>::isInitialized(contexts[i].ctxID));
		EXPECT_FALSE(ContextsPool<CGORdxContext>::isCanceling(contexts[i].ctxID));
		EXPECT_FALSE(ContextsPool<CGORdxContext>::isCanceled(contexts[i].ctxID));
		if (i < kCtxPoolSize / 2 + 1 && i != 0) {
			ASSERT_TRUE(contexts[i].next != nullptr);
			EXPECT_TRUE(ContextsPool<CGORdxContext>::isInitialized(contexts[i].next->ctxID));
			EXPECT_FALSE(ContextsPool<CGORdxContext>::isCanceling(contexts[i].next->ctxID));
			EXPECT_FALSE(ContextsPool<CGORdxContext>::isCanceled(contexts[i].next->ctxID));
			ASSERT_TRUE(contexts[i].next->next == nullptr);
		} else {
			ASSERT_TRUE(contexts[i].next == nullptr);
		}
	}

	// Cancel some of the ctx
	EXPECT_FALSE(pool->cancelContext(0, CancelType::Explicit));
	for (uint64_t i = 1; i < kCtxPoolSize / 2 + 1; ++i) {
		ASSERT_TRUE(pool->cancelContext(i, CancelType::Explicit));
		EXPECT_EQ(ctxPtrs[i - 1]->checkCancel(), CancelType::Explicit);
	}

	auto validateAfterCancel = [&]() {
		for (size_t i = 0; i < kCtxPoolSize; ++i) {
			if (i < kCtxPoolSize / 2 + 1 && i != 0) {
				EXPECT_TRUE(ContextsPool<CGORdxContext>::isInitialized(contexts[i].ctxID));
				EXPECT_FALSE(ContextsPool<CGORdxContext>::isCanceling(contexts[i].ctxID));
				EXPECT_TRUE(ContextsPool<CGORdxContext>::isCanceled(contexts[i].ctxID));

				ASSERT_TRUE(contexts[i].next != nullptr);
				EXPECT_TRUE(ContextsPool<CGORdxContext>::isInitialized(contexts[i].next->ctxID));
				EXPECT_FALSE(ContextsPool<CGORdxContext>::isCanceling(contexts[i].next->ctxID));
				EXPECT_FALSE(ContextsPool<CGORdxContext>::isCanceled(contexts[i].next->ctxID));
				ASSERT_TRUE(contexts[i].next->next == nullptr);
			} else {
				EXPECT_TRUE(ContextsPool<CGORdxContext>::isInitialized(contexts[i].ctxID));
				EXPECT_FALSE(ContextsPool<CGORdxContext>::isCanceling(contexts[i].ctxID));
				EXPECT_FALSE(ContextsPool<CGORdxContext>::isCanceled(contexts[i].ctxID));
				ASSERT_TRUE(contexts[i].next == nullptr);
			}
		}
	};
	validateAfterCancel();

	// Cancel the same contexts
	for (uint64_t i = 1; i < kCtxPoolSize / 2 + 1; ++i) {
		ASSERT_TRUE(pool->cancelContext(i, CancelType::Explicit));
	}
	validateAfterCancel();

	// Get even more contexts
	for (uint64_t i = kSecondIterCount; i < kThirdIterCount; ++i) {
		ASSERT_TRUE(getAndValidateCtx(i, *pool) != nullptr);
	}

	for (size_t i = 0; i < kCtxPoolSize; ++i) {
		EXPECT_TRUE(ContextsPool<CGORdxContext>::isInitialized(contexts[i].ctxID));
		EXPECT_FALSE(ContextsPool<CGORdxContext>::isCanceling(contexts[i].ctxID));
		ASSERT_TRUE(contexts[i].next != nullptr);
		EXPECT_TRUE(ContextsPool<CGORdxContext>::isInitialized(contexts[i].next->ctxID));
		EXPECT_FALSE(ContextsPool<CGORdxContext>::isCanceling(contexts[i].next->ctxID));
		EXPECT_FALSE(ContextsPool<CGORdxContext>::isCanceled(contexts[i].next->ctxID));
		if (i < kCtxPoolSize / 2 + 1 && i != 0) {
			EXPECT_TRUE(ContextsPool<CGORdxContext>::isCanceled(contexts[i].ctxID));
			ASSERT_TRUE(contexts[i].next->next != nullptr);
			EXPECT_TRUE(ContextsPool<CGORdxContext>::isInitialized(contexts[i].next->next->ctxID));
			EXPECT_FALSE(ContextsPool<CGORdxContext>::isCanceling(contexts[i].next->next->ctxID));
			EXPECT_FALSE(ContextsPool<CGORdxContext>::isCanceled(contexts[i].next->next->ctxID));
			ASSERT_TRUE(contexts[i].next->next->next == nullptr);
		} else {
			EXPECT_FALSE(ContextsPool<CGORdxContext>::isCanceled(contexts[i].ctxID));
			ASSERT_TRUE(contexts[i].next->next == nullptr);
		}
	}

	// Remove some of the contexts
	EXPECT_FALSE(pool->removeContext(0));
	for (uint64_t i = 1; i < kFirstIterCount; ++i) {
		ASSERT_TRUE(pool->removeContext(i));
	}

	auto validateAfterRemove = [&]() {
		for (size_t i = 0; i < kCtxPoolSize; ++i) {
			EXPECT_EQ(contexts[i].ctxID, 0);
			ASSERT_TRUE(contexts[i].next != nullptr);
			EXPECT_TRUE(ContextsPool<CGORdxContext>::isInitialized(contexts[i].next->ctxID));
			EXPECT_FALSE(ContextsPool<CGORdxContext>::isCanceling(contexts[i].next->ctxID));
			EXPECT_FALSE(ContextsPool<CGORdxContext>::isCanceled(contexts[i].next->ctxID));
			if (i < kCtxPoolSize / 2 + 1 && i != 0) {
				ASSERT_TRUE(contexts[i].next->next != nullptr);
				EXPECT_TRUE(ContextsPool<CGORdxContext>::isInitialized(contexts[i].next->next->ctxID));
				EXPECT_FALSE(ContextsPool<CGORdxContext>::isCanceling(contexts[i].next->next->ctxID));
				EXPECT_FALSE(ContextsPool<CGORdxContext>::isCanceled(contexts[i].next->next->ctxID));
				ASSERT_TRUE(contexts[i].next->next->next == nullptr);
			} else {
				ASSERT_TRUE(contexts[i].next->next == nullptr);
			}
		}
	};
	validateAfterRemove();

	// Remove the same of the contexts
	for (uint64_t i = 1; i < kFirstIterCount; ++i) {
		ASSERT_FALSE(pool->removeContext(i));
	}
	validateAfterRemove();

	// Remove the rest of the contexts
	for (uint64_t i = kFirstIterCount; i < kThirdIterCount; ++i) {
		ASSERT_TRUE(pool->removeContext(i));
	}
	for (size_t i = 0; i < kCtxPoolSize; ++i) {
		EXPECT_EQ(contexts[i].ctxID, 0);
		ASSERT_TRUE(contexts[i].next != nullptr);
		EXPECT_EQ(contexts[i].next->ctxID, 0);
		if (i < kCtxPoolSize / 2 + 1 && i != 0) {
			ASSERT_TRUE(contexts[i].next->next != nullptr);
			EXPECT_EQ(contexts[i].next->next->ctxID, 0);
			ASSERT_TRUE(contexts[i].next->next->next == nullptr);
		} else {
			ASSERT_TRUE(contexts[i].next->next == nullptr);
		}
	}
}

TEST_F(CGOCtxPoolApi, MultiThread) {
	using std::thread;

	static const size_t kThreadsCount = 16;

	auto pool = createCtxPool(kCtxPoolSize);

	vector<thread> threads;
	threads.reserve(kThreadsCount);
	std::condition_variable cond;
	std::mutex mtx;
	for (size_t i = 0; i < kThreadsCount; ++i) {
		threads.emplace_back(thread([this, &pool, &cond, &mtx]() {
			std::unique_lock<std::mutex> lck(mtx);
			cond.wait(lck);
			lck.unlock();
			static const size_t kCtxCount = kCtxPoolSize * 2;
			vector<IRdxCancelContext*> ctxPtrs(kCtxCount);
			for (uint64_t i = 0; i < kCtxCount; ++i) {
				ctxPtrs[i] = getAndValidateCtx(i + 1, *pool);
			}
			for (uint64_t i = 0; i < kCtxCount; ++i) {
				if (ctxPtrs[i] && i % 2 == 0) {
					EXPECT_TRUE(pool->cancelContext(i + 1, CancelType::Explicit));
					EXPECT_EQ(ctxPtrs[i]->checkCancel(), CancelType::Explicit);
				}
			}
			for (uint64_t i = 0; i < kCtxCount; ++i) {
				if (ctxPtrs[i]) {
					EXPECT_TRUE(pool->removeContext(i + 1));
				}
			}
		}));
	}

	std::this_thread::sleep_for(std::chrono::seconds(2));
	std::unique_lock<std::mutex> lck(mtx);
	cond.notify_all();
	lck.unlock();
	for (auto& thread : threads) {
		thread.join();
	}

	auto& contexts = pool->contexts();
	for (size_t i = 0; i < kCtxPoolSize; ++i) {
		auto node = &contexts[i];
		ASSERT_TRUE(node->next != nullptr);
		do {
			EXPECT_EQ(node->ctxID, 0);
			node = node->next;
		} while (node);
	}
}

TEST_F(CGOCtxPoolApi, ConcurrentCancel) {
	using std::thread;

	static const size_t kGetThreadsCount = 8;
	static const size_t kCancelThreadsCount = 8;

	auto pool = createCtxPool(kCtxPoolSize);

	vector<thread> threads;
	threads.reserve(kGetThreadsCount + kCancelThreadsCount);
	std::condition_variable cond;
	std::mutex mtx;
	for (size_t i = 0; i < kGetThreadsCount; ++i) {
		threads.emplace_back(thread([i, &pool, &cond, &mtx]() {
			size_t threadID = i;
			std::unique_lock<std::mutex> lck(mtx);
			cond.wait(lck);
			lck.unlock();
			static const size_t kCtxCount = 2 * kCtxPoolSize / kGetThreadsCount;
			vector<IRdxCancelContext*> ctxPtrs(kCtxCount);
			for (uint64_t i = kCtxCount * threadID, j = 0; i < kCtxCount * (threadID + 1); ++i, ++j) {
				ctxPtrs[j] = pool->getContext(i + 1);
				ASSERT_TRUE(ctxPtrs[j] != nullptr);
			}

			for (uint64_t i = kCtxCount * threadID, j = 0; i < kCtxCount * (threadID + 1); ++i, ++j) {
				while (ctxPtrs[j]->checkCancel() == CancelType::None) {
					std::this_thread::yield();
				}
				EXPECT_TRUE(pool->removeContext(i + 1));
			}
		}));
	}

	for (size_t i = 0; i < kCancelThreadsCount; ++i) {
		threads.emplace_back(thread([i, &pool, &cond, &mtx]() {
			size_t threadID = i;
			std::unique_lock<std::mutex> lck(mtx);
			cond.wait(lck);
			lck.unlock();
			static const size_t kCtxCount = 2 * kCtxPoolSize / kCancelThreadsCount;
			for (uint64_t i = kCtxCount * threadID; i < kCtxCount * (threadID + 1); ++i) {
				while (!pool->cancelContext(i + 1, CancelType::Explicit)) {
					std::this_thread::yield();
				}
			}
		}));
	}

	std::this_thread::sleep_for(std::chrono::seconds(2));
	std::unique_lock<std::mutex> lck(mtx);
	cond.notify_all();
	lck.unlock();
	for (auto& thread : threads) {
		thread.join();
	}

	auto& contexts = pool->contexts();
	for (size_t i = 0; i < kCtxPoolSize; ++i) {
		auto node = &contexts[i];
		do {
			EXPECT_EQ(node->ctxID, 0);
			node = node->next;
		} while (node);
	}
}

// Just for tsan check
TEST_F(CGOCtxPoolApi, GeneralConcurrencyCheck) {
	using std::thread;

	static const size_t kGetThreadsCount = 8;
	static const size_t kRemoveThreadsCount = 8;
	static const size_t kCancelThreadsCount = 8;

	auto pool = createCtxPool(kCtxPoolSize);

	vector<thread> threads;
	threads.reserve(kGetThreadsCount + kCancelThreadsCount + kRemoveThreadsCount);
	std::condition_variable cond;
	std::mutex mtx;
	for (size_t i = 0; i < kGetThreadsCount; ++i) {
		threads.emplace_back(thread([&pool, &cond, &mtx]() {
			std::unique_lock<std::mutex> lck(mtx);
			cond.wait(lck);
			lck.unlock();
			static const size_t kCtxCount = 2 * kCtxPoolSize;
			for (uint64_t i = 1; i <= kCtxCount; ++i) {
				pool->getContext(i);
			}
		}));
	}

	for (size_t i = 0; i < kCancelThreadsCount; ++i) {
		threads.emplace_back(thread([&pool, &cond, &mtx]() {
			std::unique_lock<std::mutex> lck(mtx);
			cond.wait(lck);
			lck.unlock();
			static const size_t kCtxCount = 2 * kCtxPoolSize;
			for (uint64_t i = 1; i <= kCtxCount; ++i) {
				pool->cancelContext(i, CancelType::Explicit);
			}
		}));
	}

	for (size_t i = 0; i < kRemoveThreadsCount; ++i) {
		threads.emplace_back(thread([&pool, &cond, &mtx]() {
			std::unique_lock<std::mutex> lck(mtx);
			cond.wait(lck);
			lck.unlock();
			static const size_t kCtxCount = 2 * kCtxPoolSize;
			for (uint64_t i = 1; i <= kCtxCount; ++i) {
				pool->removeContext(i);
			}
		}));
	}

	std::this_thread::sleep_for(std::chrono::seconds(2));
	std::unique_lock<std::mutex> lck(mtx);
	cond.notify_all();
	lck.unlock();
	for (auto& thread : threads) {
		thread.join();
	}
}