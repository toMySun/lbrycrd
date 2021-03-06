// Copyright (c) 2015-2019 The LBRY Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://opensource.org/licenses/mit-license.php

#include <test/claimtriefixture.h>

using namespace std;

void ValidatePairs(CClaimTrieCache& cache, const std::vector<std::pair<bool, uint256>>& pairs, uint256 claimHash)
{
    for (auto& pair : pairs)
        if (pair.first) // we're on the right because we were an odd index number
            claimHash = Hash(pair.second.begin(), pair.second.end(), claimHash.begin(), claimHash.end());
        else
            claimHash = Hash(claimHash.begin(), claimHash.end(), pair.second.begin(), pair.second.end());

    BOOST_CHECK_EQUAL(cache.getMerkleHash(), claimHash);
}

BOOST_FIXTURE_TEST_SUITE(claimtriehashfork_tests, RegTestingSetup)

BOOST_AUTO_TEST_CASE(hash_includes_all_claims_rollback_test)
{
    ClaimTrieChainFixture fixture;
    fixture.setHashForkHeight(5);

    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), "test", "one", 1);
    fixture.IncrementBlocks(1);

    uint256 currentRoot = fixture.getMerkleHash();
    fixture.IncrementBlocks(1);
    BOOST_CHECK_EQUAL(currentRoot, fixture.getMerkleHash());
    fixture.IncrementBlocks(3);
    BOOST_CHECK_NE(currentRoot, fixture.getMerkleHash());
    fixture.DecrementBlocks(3);
    BOOST_CHECK_EQUAL(currentRoot, fixture.getMerkleHash());
}

BOOST_AUTO_TEST_CASE(hash_includes_all_claims_single_test)
{
    ClaimTrieChainFixture fixture;
    fixture.setHashForkHeight(2);
    fixture.IncrementBlocks(4);

    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), "test", "one", 1);
    fixture.IncrementBlocks(1);

    COutPoint outPoint(tx1.GetHash(), 0);
    uint160 claimId = ClaimIdHash(tx1.GetHash(), 0);

    CClaimTrieProof proof;
    BOOST_CHECK(fixture.getProofForName("test", proof, [&claimId](const CClaimValue& claim) {
        return claim.claimId == claimId;
    }));
    BOOST_CHECK(proof.hasValue);
    BOOST_CHECK_EQUAL(proof.outPoint, outPoint);
    auto claimHash = getValueHash(outPoint, proof.nHeightOfLastTakeover);
    ValidatePairs(fixture, proof.pairs, claimHash);
}

BOOST_AUTO_TEST_CASE(hash_includes_all_claims_triple_test)
{
    ClaimTrieChainFixture fixture;
    fixture.setHashForkHeight(2);
    fixture.IncrementBlocks(4);

    std::string names[] = {"test", "tester", "tester2"};
    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), names[0], "one", 1);
    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), names[0], "two", 2);
    CMutableTransaction tx3 = fixture.MakeClaim(fixture.GetCoinbase(), names[0], "thr", 3);
    CMutableTransaction tx7 = fixture.MakeClaim(fixture.GetCoinbase(), names[0], "for", 4);
    CMutableTransaction tx8 = fixture.MakeClaim(fixture.GetCoinbase(), names[0], "fiv", 5);
    CMutableTransaction tx4 = fixture.MakeClaim(fixture.GetCoinbase(), names[1], "two", 2);
    CMutableTransaction tx5 = fixture.MakeClaim(fixture.GetCoinbase(), names[1], "thr", 3);
    CMutableTransaction tx6 = fixture.MakeClaim(fixture.GetCoinbase(), names[2], "one", 1);
    fixture.IncrementBlocks(1);

    for (const auto& name : names) {
        for (auto& claimSupports : fixture.getClaimsForName(name).claimsNsupports) {
            CClaimTrieProof proof;
            auto& claim = claimSupports.claim;
            BOOST_CHECK(fixture.getProofForName(name, proof, [&claim](const CClaimValue& value) {
                return claim.claimId == value.claimId;
            }));
            BOOST_CHECK(proof.hasValue);
            BOOST_CHECK_EQUAL(proof.outPoint, claim.outPoint);
            uint256 claimHash = getValueHash(claim.outPoint, proof.nHeightOfLastTakeover);
            ValidatePairs(fixture, proof.pairs, claimHash);
        }
    }
}

BOOST_AUTO_TEST_CASE(hash_includes_all_claims_branched_test)
{
    ClaimTrieChainFixture fixture;
    fixture.setHashForkHeight(2);
    fixture.IncrementBlocks(4);

    std::string names[] = {"test", "toast", "tot", "top", "toa", "toad"};
    for (const auto& name : names)
        fixture.MakeClaim(fixture.GetCoinbase(), name, "one", 1);

    fixture.MakeClaim(fixture.GetCoinbase(), "toa", "two", 2);
    fixture.MakeClaim(fixture.GetCoinbase(), "toa", "tre", 3);
    fixture.MakeClaim(fixture.GetCoinbase(), "toa", "qua", 4);
    fixture.MakeClaim(fixture.GetCoinbase(), "toa", "cin", 5);
    fixture.IncrementBlocks(1);

    for (const auto& name : names) {
        for (auto& claimSupports : fixture.getClaimsForName(name).claimsNsupports) {
            CClaimTrieProof proof;
            auto& claim = claimSupports.claim;
            BOOST_CHECK(fixture.getProofForName(name, proof, [&claim](const CClaimValue& value) {
                return claim.claimId == value.claimId;
            }));
            BOOST_CHECK(proof.hasValue);
            BOOST_CHECK_EQUAL(proof.outPoint, claim.outPoint);
            uint256 claimHash = getValueHash(claim.outPoint, proof.nHeightOfLastTakeover);
            ValidatePairs(fixture, proof.pairs, claimHash);
        }
    }
}

BOOST_AUTO_TEST_CASE(hash_claims_children_fuzzer_test)
{
    ClaimTrieChainFixture fixture;
    fixture.setHashForkHeight(2);
    fixture.IncrementBlocks(4);

    std::size_t i = 0;
    auto names = random_strings(300);
    auto lastTx = MakeTransactionRef(fixture.GetCoinbase());
    for (const auto& name : names) {
        auto tx = fixture.MakeClaim(*lastTx, name, "one", 1);
        lastTx = MakeTransactionRef(std::move(tx));
        if (++i % 5 == 0)
            for (std::size_t j = 0;  j < (i / 5); ++j) {
                auto tx = fixture.MakeClaim(*lastTx, name, "one", 1);
                lastTx = MakeTransactionRef(std::move(tx));
            }
        fixture.IncrementBlocks(1);
    }

    for (const auto& name : names) {
        for (auto& claimSupports : fixture.getClaimsForName(name).claimsNsupports) {
            CClaimTrieProof proof;
            auto& claim = claimSupports.claim;
            BOOST_CHECK(fixture.getProofForName(name, proof, [&claim](const CClaimValue& value) {
                return claim.claimId == value.claimId;
            }));
            BOOST_CHECK(proof.hasValue);
            BOOST_CHECK_EQUAL(proof.outPoint, claim.outPoint);
            uint256 claimHash = getValueHash(claim.outPoint, proof.nHeightOfLastTakeover);
            ValidatePairs(fixture, proof.pairs, claimHash);
        }
    }
}

bool verify_proof(const CClaimTrieProof proof, uint256 rootHash, const std::string& name)
{
    uint256 previousComputedHash;
    std::string computedReverseName;
    bool verifiedValue = false;

    for (auto itNodes = proof.nodes.rbegin(); itNodes != proof.nodes.rend(); ++itNodes) {
        bool foundChildInChain = false;
        std::vector<unsigned char> vchToHash;
        for (auto itChildren = itNodes->children.begin(); itChildren != itNodes->children.end(); ++itChildren) {
            vchToHash.push_back(itChildren->first);
            uint256 childHash;
            if (itChildren->second.IsNull()) {
                if (previousComputedHash.IsNull()) {
                    return false;
                }
                if (foundChildInChain) {
                    return false;
                }
                foundChildInChain = true;
                computedReverseName += itChildren->first;
                childHash = previousComputedHash;
            } else {
                childHash = itChildren->second;
            }
            vchToHash.insert(vchToHash.end(), childHash.begin(), childHash.end());
        }
        if (itNodes != proof.nodes.rbegin() && !foundChildInChain) {
            return false;
        }
        if (itNodes->hasValue) {
            uint256 valHash;
            if (itNodes->valHash.IsNull()) {
                if (itNodes != proof.nodes.rbegin()) {
                    return false;
                }
                if (!proof.hasValue) {
                    return false;
                }
                valHash = getValueHash(proof.outPoint,
                                       proof.nHeightOfLastTakeover);

                verifiedValue = true;
            } else {
                valHash = itNodes->valHash;
            }
            vchToHash.insert(vchToHash.end(), valHash.begin(), valHash.end());
        } else if (proof.hasValue && itNodes == proof.nodes.rbegin()) {
            return false;
        }
        CHash256 hasher;
        std::vector<unsigned char> vchHash(hasher.OUTPUT_SIZE);
        hasher.Write(vchToHash.data(), vchToHash.size());
        hasher.Finalize(&(vchHash[0]));
        uint256 calculatedHash(vchHash);
        previousComputedHash = calculatedHash;
    }
    if (previousComputedHash != rootHash) {
        return false;
    }
    if (proof.hasValue && !verifiedValue) {
        return false;
    }
    std::string::reverse_iterator itComputedName = computedReverseName.rbegin();
    std::string::const_iterator itName = name.begin();
    for (; itName != name.end() && itComputedName != computedReverseName.rend(); ++itName, ++itComputedName) {
        if (*itName != *itComputedName) {
            return false;
        }
    }
    return (!proof.hasValue || itName == name.end());
}

BOOST_AUTO_TEST_CASE(value_proof_test)
{
    ClaimTrieChainFixture fixture;

    std::string sName1("a");
    std::string sValue1("testa");

    std::string sName2("abc");
    std::string sValue2("testabc");

    std::string sName3("abd");
    std::string sValue3("testabd");

    std::string sName4("zyx");
    std::string sValue4("testzyx");

    std::string sName5("zyxa");
    std::string sName6("omg");
    std::string sName7("");

    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), sName1, sValue1);
    COutPoint tx1OutPoint(tx1.GetHash(), 0);

    CMutableTransaction tx2 = fixture.MakeClaim(fixture.GetCoinbase(), sName2, sValue2);
    COutPoint tx2OutPoint(tx2.GetHash(), 0);

    CMutableTransaction tx3 = fixture.MakeClaim(fixture.GetCoinbase(), sName3, sValue3);
    COutPoint tx3OutPoint(tx3.GetHash(), 0);

    CMutableTransaction tx4 = fixture.MakeClaim(fixture.GetCoinbase(), sName4, sValue4);
    COutPoint tx4OutPoint(tx4.GetHash(), 0);
    CClaimValue val;

    // create a claim. verify the expiration event has been scheduled.
    fixture.IncrementBlocks(5, true);

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
    BOOST_CHECK(fixture.getInfoForName(sName1, val));
    BOOST_CHECK_EQUAL(val.outPoint, tx1OutPoint);
    BOOST_CHECK(fixture.getInfoForName(sName2, val));
    BOOST_CHECK_EQUAL(val.outPoint, tx2OutPoint);
    BOOST_CHECK(fixture.getInfoForName(sName3, val));
    BOOST_CHECK_EQUAL(val.outPoint, tx3OutPoint);
    BOOST_CHECK(fixture.getInfoForName(sName4, val));
    BOOST_CHECK_EQUAL(val.outPoint, tx4OutPoint);

    CClaimTrieProof proof;

    BOOST_CHECK(fixture.getProofForName(sName1, proof));
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName1));
    BOOST_CHECK_EQUAL(proof.outPoint, tx1OutPoint);

    BOOST_CHECK(fixture.getProofForName(sName2, proof));
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName2));
    BOOST_CHECK_EQUAL(proof.outPoint, tx2OutPoint);

    BOOST_CHECK(fixture.getProofForName(sName3, proof));
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName3));
    BOOST_CHECK_EQUAL(proof.outPoint, tx3OutPoint);

    BOOST_CHECK(fixture.getProofForName(sName4, proof));
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName4));
    BOOST_CHECK_EQUAL(proof.outPoint, tx4OutPoint);

    BOOST_CHECK(fixture.getProofForName(sName5, proof));
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName5));
    BOOST_CHECK_EQUAL(proof.hasValue, false);

    BOOST_CHECK(fixture.getProofForName(sName6, proof));
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName6));
    BOOST_CHECK_EQUAL(proof.hasValue, false);

    BOOST_CHECK(fixture.getProofForName(sName7, proof));
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName7));
    BOOST_CHECK_EQUAL(proof.hasValue, false);

    CMutableTransaction tx5 = fixture.MakeClaim(fixture.GetCoinbase(), sName7, sValue4);
    COutPoint tx5OutPoint(tx5.GetHash(), 0);

    fixture.IncrementBlocks(1);

    BOOST_CHECK(fixture.getInfoForName(sName7, val));
    BOOST_CHECK_EQUAL(val.outPoint, tx5OutPoint);

    BOOST_CHECK(fixture.getProofForName(sName1, proof));
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName1));
    BOOST_CHECK_EQUAL(proof.outPoint, tx1OutPoint);

    BOOST_CHECK(fixture.getProofForName(sName2, proof));
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName2));
    BOOST_CHECK_EQUAL(proof.outPoint, tx2OutPoint);

    BOOST_CHECK(fixture.getProofForName(sName3, proof));
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName3));
    BOOST_CHECK_EQUAL(proof.outPoint, tx3OutPoint);

    BOOST_CHECK(fixture.getProofForName(sName4, proof));
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName4));
    BOOST_CHECK_EQUAL(proof.outPoint, tx4OutPoint);

    BOOST_CHECK(fixture.getProofForName(sName5, proof));
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName5));
    BOOST_CHECK_EQUAL(proof.hasValue, false);

    BOOST_CHECK(fixture.getProofForName(sName6, proof));
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName6));
    BOOST_CHECK_EQUAL(proof.hasValue, false);

    BOOST_CHECK(fixture.getProofForName(sName7, proof));
    BOOST_CHECK(verify_proof(proof, chainActive.Tip()->hashClaimTrie, sName7));
    BOOST_CHECK_EQUAL(proof.outPoint, tx5OutPoint);

    fixture.DecrementBlocks();
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(fixture.queueEmpty());
}

// Check that blocks with bogus calimtrie hash is rejected
BOOST_AUTO_TEST_CASE(bogus_claimtrie_hash_test)
    {
            ClaimTrieChainFixture fixture;
    std::string sName("test");
    std::string sValue1("test");

    int orig_chain_height = chainActive.Height();

    CMutableTransaction tx1 = fixture.MakeClaim(fixture.GetCoinbase(), sName, sValue1, 1);

    std::unique_ptr<CBlockTemplate> pblockTemp;
    BOOST_CHECK(pblockTemp = AssemblerForTest().CreateNewBlock(tx1.vout[0].scriptPubKey));
    pblockTemp->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    pblockTemp->block.nVersion = 5;
    pblockTemp->block.nTime = chainActive.Tip()->GetBlockTime() + Params().GetConsensus().nPowTargetSpacing;
    CMutableTransaction txCoinbase(*pblockTemp->block.vtx[0]);
    txCoinbase.vin[0].scriptSig = CScript() << int(chainActive.Height() + 1) << 1;
    txCoinbase.vout[0].nValue = GetBlockSubsidy(chainActive.Height() + 1, Params().GetConsensus());
    pblockTemp->block.vtx[0] = MakeTransactionRef(std::move(txCoinbase));
    pblockTemp->block.hashMerkleRoot = BlockMerkleRoot(pblockTemp->block);
    //create bogus hash

    uint256 bogusHashClaimTrie;
    bogusHashClaimTrie.SetHex("aaa");
    pblockTemp->block.hashClaimTrie = bogusHashClaimTrie;

    for (uint32_t i = 0;; ++i) {
        pblockTemp->block.nNonce = i;
        if (CheckProofOfWork(pblockTemp->block.GetPoWHash(), pblockTemp->block.nBits, Params().GetConsensus())) {
            break;
        }
    }
    bool success = ProcessNewBlock(Params(), std::make_shared<const CBlock>(pblockTemp->block), true, nullptr);
    // will process , but will not be connected
    BOOST_CHECK(success);
    BOOST_CHECK(pblockTemp->block.GetHash() != chainActive.Tip()->GetBlockHash());
    BOOST_CHECK_EQUAL(orig_chain_height, chainActive.Height());
}

BOOST_AUTO_TEST_SUITE_END()
