#include <iostream>
#include <vector>
#include <ctime>
#include <queue>
#include <cstring>
#include <cassert>
#include <cstdint>
#include "Block.h"
#include "Transaction.h"
#include "Utility.h"
using namespace std;

Block::Block() : Block(NULL) {}

Block::Block(const Block * _previousBlock) : previousBlock(_previousBlock) {
	assert(0 < bits && bits < 256); // 2진수 기준 난이도 범위

	if (previousBlock != NULL)
		memcpy(previousBlockHash, previousBlock->blockHash, sizeof(previousBlockHash));
	else
		memset(previousBlockHash, 0, sizeof(previousBlockHash));
}

/* 2진수 기준 blockHash 앞에 0이 bits개 이상 있으면 true, 그 외 false. */
bool Block::miningSuccess() const {
	int byte = bits / 8;
	int remainBits = bits - 8 * byte;

	int i;
	for (i = 0; i < byte; i++) {
		if (blockHash[i] != 0)
			return false;
	}

	if ((uint8_t)blockHash[i] > pow(2, 8 - remainBits) - 1)
		return false;
	
	return true;
}

void Block::mining() {
	int i = sizeof(version) + sizeof(previousBlockHash) + sizeof(merkleRoot) + sizeof(timestamp) + sizeof(bits);

MINING:
	nonce = 0;
	timestamp = time(NULL);

	uint8_t * blockHeader = createBlockHeader();
	SHA256_Encrpyt(blockHeader, getBlockHeaderSize(), blockHash);

	while (!miningSuccess()) {
		if (nonce == UINT64_MAX)
			goto MINING;

		nonce++;
		memcpy(blockHeader + i, &nonce, sizeof(nonce));
		SHA256_Encrpyt(blockHeader, getBlockHeaderSize(), blockHash);
	}

	isMainChain = true;
	if (previousBlock != NULL)
		height = previousBlock->height + 1;
	else
		height = 0;

	delete[] blockHeader;
}

/* 반환된 포인터는 나중에 delete[]로 할당 해제해야 함. */
uint8_t * Block::createBlockHeader() const {
	int i = 0;			// transactionData index
	uint8_t * blockHeader = new uint8_t[getBlockHeaderSize()];

	memcpy(blockHeader + i, &version, sizeof(version));
	i += sizeof(version);

	memcpy(blockHeader + i, previousBlockHash, sizeof(previousBlockHash));
	i += sizeof(previousBlockHash);

	memcpy(blockHeader + i, merkleRoot, sizeof(merkleRoot));
	i += sizeof(merkleRoot);

	memcpy(blockHeader + i, &bits, sizeof(bits));
	i += sizeof(bits);

	memcpy(blockHeader + i, &timestamp, sizeof(timestamp));
	i += sizeof(timestamp);

	memcpy(blockHeader + i, &nonce, sizeof(nonce));
	i += sizeof(nonce);

	assert(i == getBlockHeaderSize());

	return blockHeader;
}

/* 개발 중 
블록의 시간 간격이 적절한지
previousBlockHash가 유효한지
머클루트 계산을 정확히 했는지
채굴을 정확하게 했는지 */
bool Block::isValid() const {
	if (previousBlock != NULL) {
		if(previousBlock->timestamp - VALID_TIMESTAMP_GAP > timestamp) {
			cout << "\n\n" << height << "th block timestamp is unvalid...\n";
			return false;
		}
		if (previousBlock->height + 1 != height) {
			cout << "\n\n" << height << "th block index is unvalid...\n";
			return false;
		}
	}
		

	const uint8_t * merkleRoot = createMerkleRoot();

	if (isMemoryEqual(merkleRoot, merkleRoot, SHA256_DIGEST_VALUELEN)) {
		cout << "\n\nUnvalid transaction... Some of Transaction Data are changed...\n";
		delete[] merkleRoot;
		return false;
	}
	else {
		delete[] merkleRoot;
		return true;
	}

	uint8_t * hash = new uint8_t[SHA256_DIGEST_VALUELEN];
	const uint8_t * blockHeader = createBlockHeader();
	SHA256_Encrpyt(blockHeader, getBlockHeaderSize(), hash);

	if (isMemoryEqual(hash, blockHash, SHA256_DIGEST_VALUELEN)) {
		cout << "\n\nUnvalid block... Block Header are changed...\n";
		delete[] hash;
		delete[] blockHeader;
		return false;
	} else {
		delete[] hash;
		delete[] blockHeader;
		return true;
	}
}


void Block::initializeMerkleRoot() {
	assert(transactions.size() > 0);

	const uint8_t * _merkleRoot = createMerkleRoot();

	memcpy(merkleRoot, _merkleRoot, sizeof(merkleRoot));		// Merkle tree의 root node를 merkleRoot에 복사한다.
	delete[] _merkleRoot;
}


/* 개발 중 */
void Block::addTransactionsFrom(queue<Transaction *> & transactionPool) {
	while (transactions.size() < MAX_TRANSACTION_COUNT) {
		Transaction * ptx = transactionPool.front();

		// Tx 복사





		// transactions.push_back(transactionPool.front());
		transactionPool.pop();
	}
}


/* 반환된 포인터는 나중에 delete[]로 할당 해제해야 함.
채굴할 블록 안에 Tx가 하나도 없는 경우 NULL을 반환함. */
const uint8_t * Block::createMerkleRoot() const {
	vector<uint8_t *> txHashes;
	txHashes.reserve(MAX_TRANSACTION_COUNT);
	
	if (transactions.size() == 0)		// 블록 안에 Tx가 하나도 없는 경우
		return NULL;

	for (const Transaction & tx : transactions) {
		uint8_t * txHash = new uint8_t[SHA256_DIGEST_VALUELEN];
		memcpy(txHash, tx.txHash, SHA256_DIGEST_VALUELEN);
		txHashes.push_back(txHash);
	}

	while (txHashes.size() > 1) {
		if (txHashes.size() % 2 != 0)				// Tx 개수가 홀수이면 제일 마지막 Tx를 복사해서 짝수로 만든다.
			txHashes.push_back(txHashes.back());

		assert(txHashes.size() % 2 == 0);

		vector<uint8_t *> newTxHashes;
		newTxHashes.reserve(txHashes.size() / 2);
		
		for (auto iterator = txHashes.cbegin(); iterator != txHashes.cend(); iterator += 2) {	// const_iterator
			uint8_t hashIn[SHA256_DIGEST_VALUELEN * 2];
			memcpy(hashIn, *iterator, SHA256_DIGEST_VALUELEN);									// Tx Hash 첫 번째
			memcpy(hashIn + SHA256_DIGEST_VALUELEN, *(iterator + 1), SHA256_DIGEST_VALUELEN);	// Tx Hash 두 번째를 연결한다.
			delete[] *iterator;
			delete[] *(iterator + 1);

			uint8_t * hashOut = new uint8_t[SHA256_DIGEST_VALUELEN];		// 연결된 2개의 해시를 해싱한다.
			SHA256_Encrpyt(hashIn, SHA256_DIGEST_VALUELEN * 2, hashOut);

			newTxHashes.push_back(hashOut);									// 머클트리 형식으로 계속 해싱한다.
		}
		txHashes.swap(newTxHashes);
	}

	return txHashes[0];
}









