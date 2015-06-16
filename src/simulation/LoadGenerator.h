#pragma once

// Copyright 2015 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "main/Application.h"
#include "crypto/SecretKey.h"
#include "transactions/TxTests.h"
#include "generated/Stellar-types.h"
#include <vector>

namespace medida
{
class MetricsRegistry;
class Meter;
class Counter;
class Timer;
}

namespace stellar
{

class VirtualTimer;

class LoadGenerator
{
  public:
    LoadGenerator();
    ~LoadGenerator();

    struct TxInfo;
    struct AccountInfo;
    using AccountInfoPtr = std::shared_ptr<AccountInfo>;

    static std::string pickRandomCurrency();
    static const uint32_t STEP_MSECS;

    // Primary store of accounts.
    std::vector<AccountInfoPtr> mAccounts;

    // Subset of accounts that have issued credit in some currency.
    std::vector<AccountInfoPtr> mGateways;

    // Subset of accounts that have made offers to trade in some credits.
    std::vector<AccountInfoPtr> mMarketMakers;

    // Temporary: accounts that turst gateways but haven't been funded with
    // gateway credit yet.
    std::vector<AccountInfoPtr> mNeedFund;

    // Temporary: accounts that are market makers but haven't put in their
    // offers yet.
    std::vector<AccountInfoPtr> mNeedOffer;

    std::unique_ptr<VirtualTimer> mLoadTimer;
    int64 mMinBalance;

    // Schedule a callback to generateLoad() STEP_MSECS miliseconds from now.
    void scheduleLoadGeneration(Application& app, uint32_t nAccounts,
                                uint32_t nTxs, uint32_t txRate);

    // Generate one "step" worth of load (assuming 1 step per STEP_MSECS) at a
    // given target number of accounts and txs, and a given target tx/s rate.
    // If work remains after the current step, call scheduleLoadGeneration()
    // with the remainder.
    void generateLoad(Application& app, uint32_t nAccounts, uint32_t nTxs,
                      uint32_t txRate);

    bool maybeCreateAccount(uint32_t ledgerNum, std::vector<TxInfo> &txs);
    void fundPendingTrustlines(uint32_t ledgerNum, std::vector<TxInfo> &txs);
    void createPendingOffers(uint32_t ledgerNum, std::vector<TxInfo> &txs);

    std::vector<TxInfo> accountCreationTransactions(size_t n);
    AccountInfoPtr createAccount(size_t i, uint32_t ledgerNum = 0);
    std::vector<AccountInfoPtr> createAccounts(size_t n);
    bool loadAccount(Application& app, AccountInfo& account);

    TxInfo createTransferNativeTransaction(AccountInfoPtr from,
                                           AccountInfoPtr to, int64_t amount);

    TxInfo createTransferCreditTransaction(AccountInfoPtr from,
                                           AccountInfoPtr to, int64_t amount,
                                           AccountInfoPtr issuer);

    TxInfo createEstablishTrustTransaction(AccountInfoPtr from,
                                           AccountInfoPtr issuer);

    TxInfo createEstablishOfferTransaction(AccountInfoPtr from);

    AccountInfoPtr pickRandomAccount(AccountInfoPtr tryToAvoid,
                                     uint32_t ledgerNum);

    AccountInfoPtr pickRandomSharedTrustAccount(AccountInfoPtr from,
                                                uint32_t ledgerNum,
                                                AccountInfoPtr& issuer);

    TxInfo createRandomTransaction(float alpha, uint32_t ledgerNum = 0);
    std::vector<TxInfo> createRandomTransactions(size_t n, float paretoAlpha);
    void updateMinBalance(Application& app);

    struct TrustLineInfo
    {
        AccountInfoPtr mIssuer;
        uint32_t mLedgerEstablished;
        int64_t mBalance;
        int64_t mLimit;
    };

    struct AccountInfo : public std::enable_shared_from_this<AccountInfo>
    {
        AccountInfo(size_t id, SecretKey key, int64_t balance,
                    SequenceNumber seq, LoadGenerator& loadGen);
        size_t mId;
        SecretKey mKey;
        int64_t mBalance;
        SequenceNumber mSeq;

        // Used when this account trusts some other account's credits.
        std::vector<TrustLineInfo> mTrustLines;

        // Currency issued, if a gateway, as well as reverse maps to
        // those accounts that trust this currency and those who are
        // buying and selling it.
        std::string mIssuedCurrency;
        std::vector<AccountInfoPtr> mTrustingAccounts;
        std::vector<AccountInfoPtr> mBuyingAccounts;
        std::vector<AccountInfoPtr> mSellingAccounts;

        // Live offers, for accounts that are market makers.
        AccountInfoPtr mBuyCredit;
        AccountInfoPtr mSellCredit;

        TxInfo creationTransaction();

      private:
        LoadGenerator& mLoadGen;
    };

    struct TxMetrics
    {
        medida::Meter& mAccountCreated;
        medida::Meter& mTrustlineCreated;
        medida::Meter& mOfferCreated;
        medida::Meter& mNativePayment;
        medida::Meter& mCreditPayment;
        medida::Meter& mTxnAttempted;
        medida::Meter& mTxnRejected;

        medida::Counter& mPendingFunds;
        medida::Counter& mPendingOffers;

        TxMetrics(medida::MetricsRegistry& m);
        void report();
    };

    struct TxInfo
    {
        AccountInfoPtr mFrom;
        AccountInfoPtr mTo;
        enum
        {
            TX_CREATE_ACCOUNT,
            TX_ESTABLISH_TRUST,
            TX_ESTABLISH_OFFER,
            TX_TRANSFER_NATIVE,
            TX_TRANSFER_CREDIT
        } mType;
        int64_t mAmount;
        AccountInfoPtr mIssuer;

        bool execute(Application& app);

        void toTransactionFrames(std::vector<TransactionFramePtr>& txs
                                 TxMetrics& metrics);
        void recordExecution(int64_t baseFee);
    };
};
}
