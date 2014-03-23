///////////////////////////////////////////////////////////////////////////////
//
// Schema.hxx
//
// Copyright (c) 2013-2014 Eric Lombrozo
//
// All Rights Reserved.
//

// odb compiler complains about #pragma once
#ifndef COINDB_SCHEMA_HXX
#define COINDB_SCHEMA_HXX

#include <CoinQ_script.h>
#include <CoinQ_typedefs.h>

#include <uchar_vector.h>
#include <hash.h>
#include <CoinNodeData.h>
#include <hdkeys.h>

#include <stdutils/stringutils.h>

#include <odb/core.hxx>
#include <odb/nullable.hxx>
#include <odb/database.hxx>

#include <memory>

#include <logger.h>

#pragma db namespace session
namespace CoinDB
{

typedef odb::nullable<unsigned long> null_id_t;

#pragma db value(bytes_t) type("BLOB")

////////////////////
// SCHEMA VERSION //
////////////////////

const uint32_t SCHEMA_VERSION = 3;

#pragma db object pointer(std::shared_ptr)
class Version
{
public:
    Version(uint32_t version = SCHEMA_VERSION) : version_(version) { }

    unsigned long id() const { return id_; }

    void version(uint32_t version) { version_ = version; }
    uint32_t version() const { return version_; }

private:
    friend class odb::access;

    #pragma db id auto
    unsigned long id_;

    uint32_t version_;
};


////////////////////////////
// KEYCHAINS AND ACCOUNTS //
////////////////////////////

class Account;
class AccountBin;
class SigningScript;

#pragma db object pointer(std::shared_ptr)
class Keychain : public std::enable_shared_from_this<Keychain>
{
public:
    Keychain(const std::string& name, const secure_bytes_t& entropy, const secure_bytes_t& lock_key = secure_bytes_t(), const bytes_t& salt = bytes_t()); // Creates a new root keychain
    Keychain(const Keychain& source)
        : name_(source.name_), depth_(source.depth_), parent_fp_(source.parent_fp_), child_num_(source.child_num_), pubkey_(source.pubkey_), chain_code_(source.chain_code_), chain_code_ciphertext_(source.chain_code_ciphertext_), chain_code_salt_(source.chain_code_salt_), privkey_(source.privkey_), privkey_ciphertext_(source.privkey_ciphertext_), privkey_salt_(source.privkey_salt_), parent_(source.parent_), derivation_path_(source.derivation_path_) { }

    Keychain& operator=(const Keychain& source);

    std::shared_ptr<Keychain> get_shared_ptr() { return shared_from_this(); }

    unsigned int id() const { return id_; }

    std::string name() const { return name_; }
    void name(const std::string& name) { name_ = name; }

    std::shared_ptr<Keychain> root() { return (parent_ ? parent_->root() : shared_from_this()); }
    std::shared_ptr<Keychain> parent() const { return parent_; }
    std::shared_ptr<Keychain> child(uint32_t i, bool get_private = false);

    const std::vector<uint32_t>& derivation_path() const { return derivation_path_; }

    bool isPrivate() const { return (!privkey_.empty()) || (!privkey_ciphertext_.empty()); }

    // Lock keys must be set before persisting
    void setPrivateKeyLockKey(const secure_bytes_t& lock_key = secure_bytes_t(), const bytes_t& salt = bytes_t());
    void setChainCodeLockKey(const secure_bytes_t& lock_key = secure_bytes_t(), const bytes_t& salt = bytes_t());

    void lockPrivateKey();
    void lockChainCode();
    void lockAll();

    bool isPrivateKeyLocked() const;
    bool isChainCodeLocked() const;

    void unlockPrivateKey(const secure_bytes_t& lock_key);
    void unlockChainCode(const secure_bytes_t& lock_key);

    secure_bytes_t getSigningPrivateKey(uint32_t i, const std::vector<uint32_t>& derivation_path = std::vector<uint32_t>()) const;
    bytes_t getSigningPublicKey(uint32_t i, const std::vector<uint32_t>& derivation_path = std::vector<uint32_t>()) const;

    uint32_t depth() const { return depth_; }
    uint32_t parent_fp() const { return parent_fp_; }
    uint32_t child_num() const { return child_num_; }
    const bytes_t& pubkey() const { return pubkey_; }
    secure_bytes_t privkey() const;
    secure_bytes_t chain_code() const;

    const bytes_t& chain_code_ciphertext() const { return chain_code_ciphertext_; }
    const bytes_t& chain_code_salt() const { return chain_code_salt_; }

    // hash = ripemd160(sha256(pubkey + chain_code))
    const bytes_t& hash() const { return hash_; }

    secure_bytes_t extkey(bool get_private = false) const;

private:
    friend class odb::access;
    Keychain() { }

    #pragma db id auto
    unsigned long id_;

    #pragma db unique
    std::string name_;

    uint32_t depth_;
    uint32_t parent_fp_;
    uint32_t child_num_;
    bytes_t pubkey_;

    #pragma db transient
    secure_bytes_t chain_code_;
    bytes_t chain_code_ciphertext_;
    bytes_t chain_code_salt_;

    #pragma db transient
    secure_bytes_t privkey_;
    bytes_t privkey_ciphertext_;
    bytes_t privkey_salt_;

    #pragma db null
    std::shared_ptr<Keychain> parent_;
    std::vector<uint32_t> derivation_path_;

    #pragma db value_not_null inverse(parent_)
    std::vector<std::weak_ptr<Keychain>> children_;

    bytes_t hash_;
};

typedef std::set<std::shared_ptr<Keychain>> KeychainSet;

inline Keychain::Keychain(const std::string& name, const secure_bytes_t& entropy, const secure_bytes_t& lock_key, const bytes_t& salt)
    : name_(name)
{
    if (name.empty() || name[0] == '@') throw std::runtime_error("Invalid keychain name.");

    Coin::HDSeed hdSeed(entropy);
    Coin::HDKeychain hdKeychain(hdSeed.getMasterKey(), hdSeed.getMasterChainCode());

    depth_ = (uint32_t)hdKeychain.depth();
    parent_fp_ = hdKeychain.parent_fp();
    child_num_ = hdKeychain.child_num();
    chain_code_ = hdKeychain.chain_code();
    privkey_ = hdKeychain.key();
    pubkey_ = hdKeychain.pubkey();
    hash_ = hdKeychain.hash();

    setPrivateKeyLockKey(lock_key, salt);
    setChainCodeLockKey(lock_key, salt);
}

inline Keychain& Keychain::operator=(const Keychain& source)
{
    depth_ = source.depth_;
    parent_fp_ = source.parent_fp_;
    child_num_ = source.child_num_;
    pubkey_ = source.pubkey_;

    chain_code_ = source.chain_code_;
    chain_code_ciphertext_ = source.chain_code_ciphertext_;
    chain_code_salt_ = source.chain_code_salt_;

    privkey_ = source.privkey_;
    privkey_ciphertext_ = source.privkey_ciphertext_;
    privkey_salt_ = source.privkey_salt_;

    parent_ = source.parent_;

    derivation_path_ = source.derivation_path_;

    uchar_vector_secure hashdata = pubkey_;
    hashdata += chain_code_;
    hash_ = ripemd160(sha256(hashdata));

    return *this;
}

inline std::shared_ptr<Keychain> Keychain::child(uint32_t i, bool get_private)
{
    if (get_private && !isPrivate()) throw std::runtime_error("Cannot get private child from public keychain.");
    if (chain_code_.empty()) throw std::runtime_error("Chain code is locked.");
    if (get_private)
    {
        if (privkey_.empty()) throw std::runtime_error("Private key is locked.");
        Coin::HDKeychain hdkeychain(privkey_, chain_code_, child_num_, parent_fp_, depth_);
        hdkeychain = hdkeychain.getChild(i);
        std::shared_ptr<Keychain> child(new Keychain());;
        child->parent_ = get_shared_ptr();
        child->privkey_ = hdkeychain.privkey();
        child->pubkey_ = hdkeychain.pubkey();
        child->chain_code_ = hdkeychain.chain_code();
        child->child_num_ = hdkeychain.child_num();
        child->parent_fp_ = hdkeychain.parent_fp();
        child->depth_ = hdkeychain.depth();
        child->hash_ = hdkeychain.hash();
        child->derivation_path_ = derivation_path_;
        child->derivation_path_.push_back(i);
        return child;
    }
    else
    {
        Coin::HDKeychain hdkeychain(pubkey_, chain_code_, child_num_, parent_fp_, depth_);
        hdkeychain = hdkeychain.getChild(i);
        std::shared_ptr<Keychain> child(new Keychain());;
        child->parent_ = get_shared_ptr();
        child->pubkey_ = hdkeychain.pubkey();
        child->chain_code_ = hdkeychain.chain_code();
        child->child_num_ = hdkeychain.child_num();
        child->parent_fp_ = hdkeychain.parent_fp();
        child->depth_ = hdkeychain.depth();
        child->hash_ = hdkeychain.hash();
        child->derivation_path_ = derivation_path_;
        child->derivation_path_.push_back(i);
        return child;
    }
}

inline void Keychain::setPrivateKeyLockKey(const secure_bytes_t& /*lock_key*/, const bytes_t& salt)
{
    if (!isPrivate()) throw std::runtime_error("Cannot lock the private key of a public keychain.");
    if (privkey_.empty()) throw std::runtime_error("Key is locked.");

    // TODO: encrypt
    privkey_ciphertext_ = privkey_;
    privkey_salt_ = salt;
}

inline void Keychain::setChainCodeLockKey(const secure_bytes_t& /*lock_key*/, const bytes_t& salt)
{
    if (chain_code_.empty()) throw std::runtime_error("Chain code is locked.");

    // TODO: encrypt
    chain_code_ciphertext_ = chain_code_;
    chain_code_salt_ = salt;
}

inline void Keychain::lockPrivateKey()
{
    privkey_.clear();
}

inline void Keychain::lockChainCode()
{
    chain_code_.clear();
}

inline void Keychain::lockAll()
{
    lockPrivateKey();
    lockChainCode();
}

inline bool Keychain::isPrivateKeyLocked() const
{
    if (!isPrivate()) throw std::runtime_error("Keychain is not private.");
    return privkey_.empty();
}

inline bool Keychain::isChainCodeLocked() const
{
    return chain_code_.empty();
}

inline void Keychain::unlockPrivateKey(const secure_bytes_t& lock_key)
{
    if (!isPrivate()) throw std::runtime_error("Cannot unlock the private key of a public keychain.");
    if (!privkey_.empty()) return; // Already unlocked

    // TODO: decrypt
    privkey_ = privkey_ciphertext_;
}

inline void Keychain::unlockChainCode(const secure_bytes_t& lock_key)
{
    if (!chain_code_.empty()) return; // Already unlocked

    // TODO: decrypt
    chain_code_ = chain_code_ciphertext_;
}

inline secure_bytes_t Keychain::getSigningPrivateKey(uint32_t i, const std::vector<uint32_t>& derivation_path) const
{
    if (!isPrivate()) throw std::runtime_error("Cannot get a private signing key from public keychain.");
    if (privkey_.empty()) throw std::runtime_error("Private key is locked.");
    if (chain_code_.empty()) throw std::runtime_error("Chain code is locked.");

    Coin::HDKeychain hdkeychain(privkey_, chain_code_, child_num_, parent_fp_, depth_);
    for (auto k: derivation_path) { hdkeychain = hdkeychain.getChild(k); }
    return hdkeychain.getPrivateSigningKey(i);
}

inline bytes_t Keychain::getSigningPublicKey(uint32_t i, const std::vector<uint32_t>& derivation_path) const
{
    if (chain_code_.empty()) throw std::runtime_error("Chain code is locked.");

    Coin::HDKeychain hdkeychain(pubkey_, chain_code_, child_num_, parent_fp_, depth_);
    for (auto k: derivation_path) { hdkeychain = hdkeychain.getChild(k); }
    return hdkeychain.getPublicSigningKey(i);
}

inline secure_bytes_t Keychain::privkey() const
{
    if (!isPrivate()) throw std::runtime_error("Keychain is public.");
    if (privkey_.empty()) throw std::runtime_error("Keychain private key is locked.");
    return privkey_;
}

inline secure_bytes_t Keychain::chain_code() const
{
    if (chain_code_.empty()) throw std::runtime_error("Keychain chain code is locked.");
    return chain_code_;
}

inline secure_bytes_t Keychain::extkey(bool get_private) const
{
    if (get_private && !isPrivate()) throw std::runtime_error("Cannot get private extkey of a public keychain.");
    if (get_private && privkey_.empty()) throw std::runtime_error("Keychain private key is locked.");
    if (chain_code_.empty()) throw std::runtime_error("Keychain chain code is locked.");

    secure_bytes_t key = get_private ? privkey_ : pubkey_;
    return Coin::HDKeychain(key, chain_code_, child_num_, parent_fp_, depth_).extkey();
}

#pragma db object pointer(std::shared_ptr)
class Key
{
public:
    Key(const std::shared_ptr<Keychain>& keychain, uint32_t index);

    unsigned long id() const { return id_; }
    const bytes_t& pubkey() const { return pubkey_; }
    secure_bytes_t privkey() const;
    bool isPrivate() const { return is_private_; }


    std::shared_ptr<Keychain> root_keychain() const { return root_keychain_; }
    std::vector<uint32_t> derivation_path() const { return derivation_path_; }
    uint32_t index() const { return index_; }

private:
    friend class odb::access;

    Key() { }

    #pragma db id auto
    unsigned long id_;

    #pragma db value_not_null
    std::shared_ptr<Keychain> root_keychain_;
    std::vector<uint32_t> derivation_path_;
    uint32_t index_;

    bytes_t pubkey_;
    bool is_private_;
};

typedef std::vector<std::shared_ptr<Key>> KeyVector;

inline Key::Key(const std::shared_ptr<Keychain>& keychain, uint32_t index)
{
    root_keychain_ = keychain->root();
    derivation_path_ = keychain->derivation_path();
    index_ = index;

    is_private_ = root_keychain_->isPrivate();
    pubkey_ = keychain->getSigningPublicKey(index_);
}

inline secure_bytes_t Key::privkey() const
{
    if (!is_private_) throw std::runtime_error("Key::privkey - cannot get private key from nonprivate key object.");
    if (root_keychain_->isPrivateKeyLocked()) throw std::runtime_error("Key::privkey - private key is locked.");
    if (root_keychain_->isChainCodeLocked()) throw std::runtime_error("Key::privkey - chain code is locked.");

    return root_keychain_->getSigningPrivateKey(index_, derivation_path_);
}

#pragma db object pointer(std::shared_ptr)
class AccountBin : public std::enable_shared_from_this<AccountBin>
{
public:
    static const uint32_t CHANGE = 1;
    static const uint32_t DEFAULT = 2;

    AccountBin(std::shared_ptr<Account> account, uint32_t index, const std::string& name);

    unsigned long id() const { return id_; }

    std::shared_ptr<Account> account() const { return account_; }
    uint32_t index() const { return index_; }

    void name(const std::string& name) { name_ = name; }
    std::string name() const { return name_; }

    uint32_t script_count() const { return script_count_; }

    std::shared_ptr<SigningScript> newSigningScript(const std::string& label = "");

    bool loadKeychains();
    KeychainSet keychains() const { return keychains_; }

private:
    friend class odb::access;
    AccountBin() { }

    #pragma db id auto
    unsigned long id_;

    #pragma db value_not_null
    std::shared_ptr<Account> account_;
    uint32_t index_;
    std::string name_;

    uint32_t script_count_;

    #pragma db transient
    KeychainSet keychains_;
};

typedef std::vector<std::shared_ptr<AccountBin>> AccountBinVector;
typedef std::vector<std::weak_ptr<AccountBin>> WeakAccountBinVector;

// Immutable object containng keychain and bin names as strings
class AccountInfo
{
public:
    AccountInfo(
        unsigned long id,
        const std::string& name,
        unsigned int minsigs,
        const std::vector<std::string>& keychain_names,
        uint32_t unused_pool_size,
        uint32_t time_created,
        const std::vector<std::string>& bin_names
    ) :
        id_(id),
        name_(name),
        minsigs_(minsigs),
        keychain_names_(keychain_names),
        unused_pool_size_(unused_pool_size),
        time_created_(time_created),
        bin_names_(bin_names)
    {
        std::sort(keychain_names_.begin(), keychain_names_.end());
    }

    AccountInfo(const AccountInfo& source) :
        id_(source.id_),
        name_(source.name_),
        minsigs_(source.minsigs_),
        keychain_names_(source.keychain_names_),
        unused_pool_size_(source.unused_pool_size_),
        time_created_(source.time_created_),
        bin_names_(source.bin_names_)
    { }

    AccountInfo& operator=(const AccountInfo& source)
    {
        id_ = source.id_;
        name_ = source.name_;
        minsigs_ = source.minsigs_;
        keychain_names_ = source.keychain_names_;
        unused_pool_size_ = source.unused_pool_size_;
        time_created_ = source.time_created_;
        bin_names_ = source.bin_names_;
        return *this;
    }

    unsigned int                        id() const { return id_; }
    const std::string&                  name() const { return name_; }
    unsigned int                        minsigs() const { return minsigs_; }
    const std::vector<std::string>&     keychain_names() const { return keychain_names_; }
    uint32_t                            unused_pool_size() const { return unused_pool_size_; }
    uint32_t                            time_created() const { return time_created_; }
    const std::vector<std::string>&     bin_names() const { return bin_names_; }

private:
    unsigned long               id_;
    std::string                 name_;
    unsigned int                minsigs_;
    std::vector<std::string>    keychain_names_;
    uint32_t                    unused_pool_size_;
    uint32_t                    time_created_;
    std::vector<std::string>    bin_names_;
};

#pragma db object pointer(std::shared_ptr)
class Account : public std::enable_shared_from_this<Account>
{
public:
    Account(const std::string& name, unsigned int minsigs, const KeychainSet& keychains, uint32_t unused_pool_size = 25, uint32_t time_created = time(NULL))
        : name_(name), minsigs_(minsigs), keychains_(keychains), unused_pool_size_(unused_pool_size), time_created_(time_created)
    {
        if (name_.empty() || name[0] == '@') throw std::runtime_error("Invalid account name.");
        if (keychains_.size() > 15) throw std::runtime_error("Account can use at most 15 keychains.");
        if (minsigs > keychains_.size()) throw std::runtime_error("Account minimum signatures cannot exceed number of keychains.");
    }

    AccountInfo accountInfo() const;

    unsigned long id() const { return id_; }

    void name(const std::string& name) { name_ = name; }
    const std::string& name() const { return name_; }
    unsigned int minsigs() const { return minsigs_; }
    KeychainSet keychains() const { return keychains_; }
    uint32_t unused_pool_size() const { return unused_pool_size_; }
    uint32_t time_created() const { return time_created_; }
    AccountBinVector bins() const;

    std::shared_ptr<AccountBin> addBin(const std::string& name);

    uint32_t bin_count() const { return bins_.size(); }

private:
    friend class odb::access;
    Account() { }

    #pragma db id auto
    unsigned long id_;

    #pragma db unique
    std::string name_;
    unsigned int minsigs_;
    KeychainSet keychains_;
    uint32_t unused_pool_size_; // how many unused scripts we want in our lookahead
    uint32_t time_created_;

    #pragma db value_not_null inverse(account_)
    WeakAccountBinVector bins_;
};

inline AccountInfo Account::accountInfo() const
{
    std::vector<std::string> keychain_names;
    for (auto& keychain: keychains_) { keychain_names.push_back(keychain->name()); }

    std::vector<std::string> bin_names;
    for (auto& bin: bins()) { bin_names.push_back(bin->name()); }

    return AccountInfo(id_, name_, minsigs_, keychain_names, unused_pool_size_, time_created_, bin_names);
}

inline AccountBinVector Account::bins() const
{
    AccountBinVector bins;
    for (auto& bin: bins_) { bins.push_back(bin.lock()); }
    return bins;
}

inline std::shared_ptr<AccountBin> Account::addBin(const std::string& name)
{
    if (name.empty() || name[0] == '@') throw std::runtime_error("Invalid account bin name.");
    uint32_t index = bins_.size() + 1;
    std::shared_ptr<AccountBin> bin(new AccountBin(shared_from_this(), index, name));
    bins_.push_back(bin);
    return bin;
}

inline AccountBin::AccountBin(std::shared_ptr<Account> account, uint32_t index, const std::string& name)
    : account_(account), index_(index), name_(name), script_count_(0)
{
}

inline bool AccountBin::loadKeychains()
{
    if (!keychains_.empty()) return false;
    for (auto& keychain: account_->keychains())
    {
        std::shared_ptr<Keychain> child(keychain->child(index_));
        keychains_.insert(child);
    }
    return true;
}


#pragma db object pointer(std::shared_ptr)
class SigningScript : public std::enable_shared_from_this<SigningScript>
{
public:
    enum status_t { UNUSED = 1, CHANGE = 2, PENDING = 4, RECEIVED = 8, CANCELED = 16, ALL = 31 };
    static std::string getStatusString(int status)
    {
        std::vector<std::string> flags;
        if (status & UNUSED) flags.push_back("UNUSED");
        if (status & CHANGE) flags.push_back("CHANGE");
        if (status & PENDING) flags.push_back("PENDING");
        if (status & RECEIVED) flags.push_back("RECEIVED");
        if (status & CANCELED) flags.push_back("CANCELED");
        if (flags.empty()) return "UNKNOWN";

        return stdutils::delimited_list(flags, " | ");
    }

    static std::vector<status_t> getStatusFlags(int status)
    {
        std::vector<status_t> flags;
        if (status & UNUSED) flags.push_back(UNUSED);
        if (status & CHANGE) flags.push_back(CHANGE);
        if (status & PENDING) flags.push_back(PENDING);
        if (status & RECEIVED) flags.push_back(RECEIVED);
        if (status & CANCELED) flags.push_back(CANCELED);
        return flags;
    }

    SigningScript(std::shared_ptr<AccountBin> account_bin, uint32_t index, const std::string& label = "", status_t status = UNUSED);

    SigningScript(std::shared_ptr<AccountBin> account_bin, uint32_t index, const bytes_t& txinscript, const bytes_t& txoutscript, const std::string& label = "", status_t status = UNUSED)
        : account_(account_bin->account()), account_bin_(account_bin), index_(index), label_(label), status_(status), txinscript_(txinscript), txoutscript_(txoutscript) { }

    unsigned long id() const { return id_; }
    void label(const std::string& label) { label_ = label; }
    const std::string label() const { return label_; }

    void status(status_t status) { status_ = status; }
    status_t status() const { return status_; }

    const bytes_t& txinscript() const { return txinscript_; }
    const bytes_t& txoutscript() const { return txoutscript_; }

    std::shared_ptr<Account> account() const { return account_; }

    // 0 is reserved for subaccounts, 1 is reserved for change addresses, 2 is reserved for the default bin.
    std::shared_ptr<AccountBin> account_bin() const { return account_bin_; }

    uint32_t index() const { return index_; }

    KeyVector& keys() { return keys_; }

private:
    friend class odb::access;
    SigningScript() { }

    #pragma db id auto
    unsigned long id_;

    #pragma db value_not_null
    std::shared_ptr<Account> account_;

    #pragma db value_not_null
    std::shared_ptr<AccountBin> account_bin_;
    uint32_t index_;

    std::string label_;
    status_t status_;

    bytes_t txinscript_; // unsigned (0 byte length placeholders are used for signatures)
    bytes_t txoutscript_;

    KeyVector keys_;
};

inline SigningScript::SigningScript(std::shared_ptr<AccountBin> account_bin, uint32_t index, const std::string& label, status_t status)
    : account_(account_bin->account()), account_bin_(account_bin), index_(index), label_(label), status_(status)
{
    account_bin_->loadKeychains();
    for (auto& keychain: account_bin_->keychains())
    {
        std::shared_ptr<Key> key(new Key(keychain, index));
        keys_.push_back(key);
    }

    // sort keys into canonical order
    std::sort(keys_.begin(), keys_.end(), [](std::shared_ptr<Key> key1, std::shared_ptr<Key> key2) { return key1->pubkey() < key2->pubkey(); });

    std::vector<bytes_t> pubkeys;
    for (auto& key: keys_) { pubkeys.push_back(key->pubkey()); }
    CoinQ::Script::Script script(CoinQ::Script::Script::PAY_TO_MULTISIG_SCRIPT_HASH, account_->minsigs(), pubkeys);
    txinscript_ = script.txinscript(CoinQ::Script::Script::EDIT);
    txoutscript_ = script.txoutscript();
}

inline std::shared_ptr<SigningScript> AccountBin::newSigningScript(const std::string& label)
{
    std::shared_ptr<SigningScript> signingscript(new SigningScript(shared_from_this(), script_count_++, label));
    return signingscript;
}


// Views
#pragma db view \
    object(AccountBin) \
    object(Account: AccountBin::account_)
struct AccountBinView
{
    #pragma db column(Account::id_)
    unsigned long account_id;
    #pragma db column(Account::name_)
    std::string account_name;

    #pragma db column(AccountBin::id_)
    unsigned long bin_id; 
    #pragma db column(AccountBin::name_)
    std::string bin_name;
};

#pragma db view \
    object(SigningScript) \
    object(Account: SigningScript::account_) \
    object(AccountBin: SigningScript::account_bin_)
struct SigningScriptView
{
    #pragma db column(Account::id_)
    unsigned long account_id;

    #pragma db column(Account::name_)
    std::string account_name;

    #pragma db column(AccountBin::id_)
    unsigned long account_bin_id;

    #pragma db column(AccountBin::name_)
    std::string account_bin_name;

    #pragma db column(SigningScript::id_)
    unsigned long id;

    #pragma db column(SigningScript::label_)
    std::string label;

    #pragma db column(SigningScript::status_)
    SigningScript::status_t status;

    #pragma db column(SigningScript::txinscript_)
    bytes_t txinscript;

    #pragma db column(SigningScript::txoutscript_)
    bytes_t txoutscript;
};

#pragma db view \
    object(SigningScript) \
    object(Account: SigningScript::account_) \
    object(AccountBin: SigningScript::account_bin_)
struct ScriptCountView
{
    #pragma db column("count(" + SigningScript::id_ + ")")
    uint32_t count;
};


/*
class KeychainInfo
{
public:
    KeychainInfo(
        unsigned long id,
        const std::string& name,
        uint32_t depth,
        uint32_t parent_fp,
        uint32_t child_num,
        bytes_t pubkey,
        bytes_t hash,
        const std::string& root_name,
        const std::vector<uint32_t> derivation_path,
        const secure_bytes_t& chain_code,
        const secure_bytes_t& privkey,
        const secure_bytes_t extkey
    ) :
        id_(id),
        name_(name),
        depth_(depth),
        parent_fp_(parent_fp),
        child_num_(child_num),
        pubkey_(pubkey),
        hash_(hash),
        root_name_(root_name),
        derivation_path_(derivation_path),
        chain_code_(chain_code),
        privkey_(privkey),
        extkey_(extkey)
    { }

    KeychainInfo(const KeychainInfo& source) :
        id_(source.id_),
        name_(source.name_),
        depth_(source.depth_),
        parent_fp_(source.parent_fp_),
        child_num_(source.child_num_),
        pubkey_(source.pubkey_),
        hash_(source.hash_),
        root_name_(source.root_name_),
        derivation_path_(source.derivation_path_),
        chain_code_(source.chain_code_),
        privkey_(source.privkey_),
        extkey_(source.extkey_)
    { }

    KeychainInfo& operator=(const KeychainInfo& source)
    {
        id_ = source.id_;
        name_ = source.name_;
        depth_ = source.depth_;
        parent_fp_ = source.parent_fp_;
        child_num_ = source.child_num_;
        pubkey_ = source.pubkey_;
        hash_ = source.hash_;
        root_name_ = source.root_name_;
        derivation_path_ = source.derivation_path_;
        chain_code_ = source.chain_code_;
        privkey_ = source.privkey_;
        extkey_ = source.extkey_;
    }

    unsigned long                   id() const { return id_; }
    const std::string&              name() const { return name_; }
    uint32_t                        depth() const { return depth_; }
    uint32_t                        parent_fp() const { return parent_fp_; }
    uint32_t                        child_num() const { return child_num_; }
    const bytes_t&                  pubkey() const { return pubkey_; }
    const bytes_t&                  hash() const { return hash_; }

    const std::string&              root_name() const { return root_name_; }
    const std::vector<uint32_t>&    derivation_path() const { return derivation_path_; }

    const secure_bytes_t&           chain_code() const { return chain_code_; }
    const secure_bytes_t&           privkey() const { return privkey_; }
    const secure_bytes_t&           extkey() const { return extkey_; } 

private:
    unsigned long                   id_;
    std::string                     name_;
    uint32_t                        depth_;
    uint32_t                        parent_fp_;
    uint32_t                        child_num_;
    bytes_t                         pubkey_;
    bytes_t                         hash_;

    std::string                     root_name_;
    std::vector<uint32_t>           derivation_path_;

    secure_bytes_t                  chain_code_;
    secure_bytes_t                  privkey_;
    secure_bytes_t                  extkey_;
};
*/


/////////////////////////////
// BLOCKS AND TRANSACTIONS //
/////////////////////////////

class Tx;

#pragma db object pointer(std::shared_ptr)
class BlockHeader
{
public:
    BlockHeader() { }

    BlockHeader(const Coin::CoinBlockHeader& blockheader, uint32_t height = 0xffffffff) { fromCoinClasses(blockheader, height); }

    BlockHeader(const bytes_t& hash, uint32_t height, uint32_t version, const bytes_t& prevhash, const bytes_t& merkleroot, uint32_t timestamp, uint32_t bits, uint32_t nonce)
    : hash_(hash), height_(height), version_(version), prevhash_(prevhash), merkleroot_(merkleroot), timestamp_(timestamp), bits_(bits), nonce_(nonce) { }

    void fromCoinClasses(const Coin::CoinBlockHeader& blockheader, uint32_t height = 0xffffffff);
    Coin::CoinBlockHeader toCoinClasses() const;

    unsigned long id() const { return id_; }
    bytes_t hash() const { return hash_; }
    uint32_t height() const { return height_; }
    uint32_t version() const { return version_; }
    bytes_t prevhash() const { return prevhash_; }
    bytes_t merkleroot() const { return merkleroot_; }
    uint32_t timestamp() const { return timestamp_; }
    uint32_t bits() const { return bits_; }
    uint32_t nonce() const { return nonce_; }

private:
    friend class odb::access;

    #pragma db id auto
    unsigned long id_;

    #pragma db unique
    bytes_t hash_;

    #pragma db unique
    uint32_t height_;

    uint32_t version_;
    bytes_t prevhash_;
    bytes_t merkleroot_;
    uint32_t timestamp_;
    uint32_t bits_;
    uint32_t nonce_;
};

inline void BlockHeader::fromCoinClasses(const Coin::CoinBlockHeader& blockheader, uint32_t height)
{
    hash_ = blockheader.getHashLittleEndian();
    height_ = height;
    version_ = blockheader.version;
    prevhash_ = blockheader.prevBlockHash;
    merkleroot_ = blockheader.merkleRoot;
    timestamp_ = blockheader.timestamp;
    bits_ = blockheader.bits;
    nonce_ = blockheader.nonce;
}

inline Coin::CoinBlockHeader BlockHeader::toCoinClasses() const
{
    return Coin::CoinBlockHeader(version_, timestamp_, bits_, nonce_, prevhash_, merkleroot_);
}


#pragma db object pointer(std::shared_ptr)
class MerkleBlock
{
public:
    MerkleBlock() { }

    MerkleBlock(const std::shared_ptr<BlockHeader>& blockheader, uint32_t txcount, const std::vector<bytes_t>& hashes, const bytes_t& flags)
        : blockheader_(blockheader), txcount_(txcount), hashes_(hashes), flags_(flags) { }

    void fromCoinClasses(const Coin::MerkleBlock& merkleblock, uint32_t height = 0xffffffff);
    Coin::MerkleBlock toCoinClasses() const;

    unsigned long id() const { return id_; }

    // The logic of blockheader management and persistence is handled by the user of this class.
    void blockheader(const std::shared_ptr<BlockHeader>& blockheader) { blockheader_ = blockheader; }
    std::shared_ptr<BlockHeader> blockheader() const { return blockheader_; }

    void txcount(uint32_t txcount) { txcount_ = txcount; }
    uint32_t txcount() const { return txcount_; }

    void hashes(const std::vector<bytes_t>& hashes) { hashes_ = hashes; }
    const std::vector<bytes_t>& hashes() const { return hashes_; }

    void flags(const bytes_t& flags) { flags_ = flags; }
    const bytes_t& flags() const { return flags_; }

private:
    friend class odb::access;

    #pragma db id auto
    unsigned long id_;

    #pragma db not_null
    std::shared_ptr<BlockHeader> blockheader_;

    uint32_t txcount_;

    #pragma db value_not_null \
        id_column("object_id") value_column("value")
    std::vector<bytes_t> hashes_;

    bytes_t flags_;
};

inline void MerkleBlock::fromCoinClasses(const Coin::MerkleBlock& merkleblock, uint32_t height)
{
    blockheader_ = std::shared_ptr<BlockHeader>(new BlockHeader(merkleblock.blockHeader, height));
    txcount_ = merkleblock.nTxs;
    hashes_.assign(merkleblock.hashes.begin(), merkleblock.hashes.end());
    flags_ = merkleblock.flags;
}

inline Coin::MerkleBlock MerkleBlock::toCoinClasses() const
{
    Coin::MerkleBlock merkleblock;
    merkleblock.blockHeader = blockheader_->toCoinClasses();
    merkleblock.nTxs = txcount_;
    merkleblock.hashes.assign(hashes_.begin(), hashes_.end());
    merkleblock.flags = flags_;
    return merkleblock;
}


#pragma db object pointer(std::shared_ptr)
class TxIn
{
public:
    TxIn(const bytes_t& outhash, uint32_t outindex, const bytes_t& script, uint32_t sequence)
        : outhash_(outhash), outindex_(outindex), script_(script), sequence_(sequence) { }

    TxIn(const Coin::TxIn& coin_txin);
    TxIn(const bytes_t& raw);

    Coin::TxIn toCoinClasses() const;

    unsigned long id() const { return id_; }
    const bytes_t& outhash() const { return outhash_; }
    uint32_t outindex() const { return outindex_; }

    void script(const bytes_t& script) { script_ = script; }
    const bytes_t& script() const { return script_; }

    uint32_t sequence() const { return sequence_; }
    bytes_t raw() const;

    void tx(std::shared_ptr<Tx> tx) { tx_ = tx; }
    const std::shared_ptr<Tx> tx() const { return tx_; }

    void txindex(unsigned int txindex) { txindex_ = txindex; }
    uint32_t txindex() const { return txindex_; }

private:
    friend class odb::access;
    TxIn() { }

    #pragma db id auto
    unsigned long id_;

    bytes_t outhash_;
    uint32_t outindex_;
    bytes_t script_;
    uint32_t sequence_;

    #pragma db null
    std::shared_ptr<Tx> tx_;

    uint32_t txindex_;
};

typedef std::vector<std::shared_ptr<TxIn>> txins_t;

inline TxIn::TxIn(const Coin::TxIn& coin_txin)
{
    outhash_ = coin_txin.getOutpointHash();
    outindex_ = coin_txin.getOutpointIndex();
    script_ = coin_txin.scriptSig;
    sequence_ = coin_txin.sequence;
}

inline TxIn::TxIn(const bytes_t& raw)
{
    Coin::TxIn coin_txin(raw);
    outhash_ = coin_txin.getOutpointHash();
    outindex_ = coin_txin.getOutpointIndex();
    script_ = coin_txin.scriptSig;
    sequence_ = coin_txin.sequence;
}

inline Coin::TxIn TxIn::toCoinClasses() const
{
    Coin::TxIn coin_txin;
    coin_txin.previousOut = Coin::OutPoint(outhash_, outindex_);
    coin_txin.scriptSig = script_;
    coin_txin.sequence = sequence_;
    return coin_txin;
}

inline bytes_t TxIn::raw() const
{
    return toCoinClasses().getSerialized();
}


#pragma db object pointer(std::shared_ptr)
class TxOut
{
public:
    enum type_t { NONE = 1, CHANGE = 2, DEBIT = 4, CREDIT = 8, ALL = 15 };

    TxOut(uint64_t value, const bytes_t& script, const null_id_t& account_id = null_id_t(), type_t type = NONE)
        : value_(value), script_(script), account_id_(account_id), type_(type) { }

    TxOut(const Coin::TxOut& coin_txout, const null_id_t& account_id = null_id_t(), type_t type = NONE);
    TxOut(const bytes_t& raw, const null_id_t& account_id = null_id_t(), type_t type = NONE);

    Coin::TxOut toCoinClasses() const;

    unsigned long id() const { return id_; }
    uint64_t value() const { return value_; }
    const bytes_t& script() const { return script_; }
    bytes_t raw() const;

    void tx(std::shared_ptr<Tx> tx) { tx_ = tx; }
    const std::shared_ptr<Tx> tx() const { return tx_; }

    void txindex(uint32_t txindex) { txindex_ = txindex; }
    uint32_t txindex() const { return txindex_; }

    void spent(std::shared_ptr<TxIn> spent) { spent_ = spent; }
    const std::shared_ptr<TxIn> spent() const { return spent_; }

    void signingscript(std::shared_ptr<SigningScript> signingscript) { signingscript_ = signingscript; }
    const std::shared_ptr<SigningScript> signingscript() const { return signingscript_; }

    void account_id(const null_id_t& account_id) { account_id_ = account_id; }
    const null_id_t& account_id() const { return account_id_; }

    void type(type_t type) { type_ = type; }
    type_t type() const { return type_; }

private:
    friend class odb::access;
    TxOut() { }

    #pragma db id auto
    unsigned long id_;

    uint64_t value_;
    bytes_t script_;

    #pragma db null
    std::shared_ptr<Tx> tx_;
    uint32_t txindex_;

    #pragma db null
    std::shared_ptr<TxIn> spent_;

    #pragma db null
    std::shared_ptr<SigningScript> signingscript_;

    null_id_t account_id_;

    type_t type_;
};

typedef std::vector<std::shared_ptr<TxOut>> txouts_t;

inline TxOut::TxOut(const Coin::TxOut& coin_txout, const null_id_t& account_id, type_t type)
{
    value_ = coin_txout.value;
    script_ = coin_txout.scriptPubKey;
    account_id_ = account_id;
    type_ = type;
}

inline TxOut::TxOut(const bytes_t& raw, const null_id_t& account_id, type_t type)
{
    Coin::TxOut coin_txout(raw);
    value_ = coin_txout.value;
    script_ = coin_txout.scriptPubKey;
    account_id_ = account_id;
    type_ = type;
}

inline Coin::TxOut TxOut::toCoinClasses() const
{
    Coin::TxOut coin_txout;
    coin_txout.value = value_;
    coin_txout.scriptPubKey = script_;
    return coin_txout;
}

inline bytes_t TxOut::raw() const
{
    return toCoinClasses().getSerialized();
}


#pragma db object pointer(std::shared_ptr)
class Tx : public std::enable_shared_from_this<Tx>
{
public:
    enum status_t
    {
        UNSIGNED    = 1,      // still missing signatures
        UNSENT      = 1 << 1, // signed but not yet broadcast to network
        SENT        = 1 << 2, // sent to at least one peer but possibly not propagated
        RECEIVED    = 1 << 3, // received from at least one peer
        CONFLICTED  = 1 << 4, // unconfirmed and spends the same output as another transaction
        CANCELED    = 1 << 5, // either will never be broadcast or will never confirm
        CONFIRMED   = 1 << 6, // exists in blockchain
        ALL         = (1 << 7) - 1
    };
    /*
     * If status is UNSIGNED, remove all txinscripts before hashing so hash
     * is unchanged by adding signatures until fully signed. Then compute
     * normal hash and transition to one of the other states.
     *
     * The states are ordered such that transitions are generally from smaller values to larger values.
     * Blockchain reorgs are the exception where it's possible that a CONFIRMED state reverts to an
     * earlier state.
     */

    Tx(uint32_t version = 1, uint32_t locktime = 0, uint32_t timestamp = 0xffffffff, status_t status = RECEIVED)
        : version_(version), locktime_(locktime), timestamp_(timestamp), status_(status), have_fee_(false), fee_(0) { }

    void set(uint32_t version, const txins_t& txins, const txouts_t& txouts, uint32_t locktime, uint32_t timestamp = 0xffffffff, status_t status = RECEIVED);
    void set(const Coin::Transaction& coin_tx, uint32_t timestamp = 0xffffffff, status_t status = RECEIVED);
    void set(const bytes_t& raw, uint32_t timestamp = 0xffffffff, status_t status = RECEIVED);

    Coin::Transaction toCoinClasses() const;

    void setBlock(std::shared_ptr<BlockHeader> blockheader, uint32_t blockindex);

    unsigned long id() const { return id_; }
    const bytes_t& hash() const { return hash_; }
    bytes_t unsigned_hash() const { return unsigned_hash_; }
    txins_t txins() const;
    txouts_t txouts() const;
    uint32_t locktime() const { return locktime_; }
    bytes_t raw() const;

    void timestamp(uint32_t timestamp) { timestamp_ = timestamp; }
    uint32_t timestamp() const { return timestamp_; }

    void status(status_t status);
    status_t status() const { return status_; }

    void fee(uint64_t fee) { have_fee_ = true; fee_ = fee; }
    uint64_t fee() const { return fee_; }

    bool have_fee() const { return have_fee_; }

    void block(std::shared_ptr<BlockHeader> header, uint32_t index) { blockheader_ = header, blockindex_ = index; }

    void blockheader(std::shared_ptr<BlockHeader> blockheader) { blockheader_ = blockheader; }
    std::shared_ptr<BlockHeader> blockheader() const { return blockheader_; }

    void shuffle_txins();
    void shuffle_txouts();

    void updateStatus();
    void updateUnsignedHash();
    void updateHash();

    unsigned int missingSigCount() const;
    std::set<bytes_t> missingSigPubkeys() const;

private:
    friend class odb::access;

    void fromCoinClasses(const Coin::Transaction& coin_tx);

    #pragma db id auto
    unsigned long id_;

    // hash stays empty until transaction is fully signed.
    bytes_t hash_;

    // We'll use the unsigned hash as a unique identifier to avoid malleability issues.
    #pragma db unique
    bytes_t unsigned_hash_;

    uint32_t version_;

    #pragma db value_not_null inverse(tx_)
    std::vector<std::weak_ptr<TxIn>> txins_;

    #pragma db value_not_null inverse(tx_)
    std::vector<std::weak_ptr<TxOut>> txouts_;

    uint32_t locktime_;

    // Timestamp should be set each time we modify the transaction.
    // Once status is RECEIVED the timestamp is fixed.
    // Timestamp defaults to 0xffffffff
    uint32_t timestamp_;

    status_t status_;

    // Due to issue with odb::nullable<uint64_t> in mingw-w64, we use a second bool variable.
    bool have_fee_;
    uint64_t fee_;

    #pragma db null
    std::shared_ptr<BlockHeader> blockheader_;

    #pragma db null
    odb::nullable<uint32_t> blockindex_;
};

inline void Tx::updateStatus()
{
    // TODO: check for conflicts
    if (missingSigCount() > 0) status_ = UNSIGNED;
}

inline void Tx::updateHash()
{
    // Leave hash blank until fully signed
    if (status_ != UNSIGNED) {
        hash_ = sha256_2(raw());
        std::reverse(hash_.begin(), hash_.end());
    }
}

inline void Tx::updateUnsignedHash()
{
    Coin::Transaction coin_tx = toCoinClasses();
    coin_tx.clearScriptSigs();
    unsigned_hash_ = coin_tx.getHashLittleEndian();
}

inline void Tx::set(uint32_t version, const txins_t& txins, const txouts_t& txouts, uint32_t locktime, uint32_t timestamp, status_t status)
{
    version_ = version;

    int i = 0;
    txins_.clear();
    for (auto& txin: txins)
    {
        txin->tx(shared_from_this());
        txin->txindex(i++);
        txins_.push_back(txin);
    }

    i = 0;
    txouts_.clear();
    for (auto& txout: txouts)
    {
        txout->tx(shared_from_this());
        txout->txindex(i++);
        txouts_.push_back(txout);
    }

    locktime_ = locktime;

    timestamp_ = timestamp;
    status_ = status;

    updateStatus();
    updateUnsignedHash();
    updateHash();
}

inline void Tx::set(const Coin::Transaction& coin_tx, uint32_t timestamp, status_t status)
{
    LOGGER(trace) << "Tx::set - fromCoinClasses(coin_tx);" << std::endl;
    fromCoinClasses(coin_tx);

    timestamp_ = timestamp;
    status_ = status;

    updateStatus();
    updateUnsignedHash();
    updateHash();
}

inline void Tx::set(const bytes_t& raw, uint32_t timestamp, status_t status)
{
    Coin::Transaction coin_tx(raw);
    fromCoinClasses(coin_tx);
    timestamp_ = timestamp;
    status_ = status;

    updateStatus();
    updateUnsignedHash();
    updateHash();
}

// TODO: Status updates should occur automatically
inline void Tx::status(status_t status)
{
    if (status_ != status) {
        status_ = status;
        //updateHash();
    }
}

inline Coin::Transaction Tx::toCoinClasses() const
{
    Coin::Transaction coin_tx;
    coin_tx.version = version_;
    for (auto& txin:  txins()) { coin_tx.inputs.push_back(txin->toCoinClasses()); }
    for (auto& txout: txouts()) { coin_tx.outputs.push_back(txout->toCoinClasses()); }
    coin_tx.lockTime = locktime_;
    return coin_tx;
}

inline void Tx::setBlock(std::shared_ptr<BlockHeader> blockheader, uint32_t blockindex)
{
    blockheader_ = blockheader;
    blockindex_ = blockindex;
}

inline txins_t Tx::txins() const
{
    txins_t txins;
    for (auto& txin: txins_) { txins.push_back(txin.lock()); }
    return txins;
}

inline txouts_t Tx::txouts() const
{
    txouts_t txouts;
    for (auto& txout: txouts_) { txouts.push_back(txout.lock()); }
    return txouts;
}

inline bytes_t Tx::raw() const
{
    return toCoinClasses().getSerialized();
}

inline void Tx::shuffle_txins()
{
    int i = 0;
    std::random_shuffle(txins_.begin(), txins_.end());
    for (auto& txin: txins()) { txin->txindex(i++); }
}

inline void Tx::shuffle_txouts()
{
    int i = 0;
    std::random_shuffle(txouts_.begin(), txouts_.end());
    for (auto& txout: txouts()) { txout->txindex(i++); }
}

inline void Tx::fromCoinClasses(const Coin::Transaction& coin_tx)
{
    version_ = coin_tx.version;

    int i = 0;
    txins_.clear();
    for (auto& coin_txin: coin_tx.inputs) {
        std::shared_ptr<TxIn> txin(new TxIn(coin_txin));
        txin->tx(shared_from_this());
        txin->txindex(i++);
        txins_.push_back(txin);
    }

    i = 0;
    txouts_.clear();
    for (auto& coin_txout: coin_tx.outputs) {
        std::shared_ptr<TxOut> txout(new TxOut(coin_txout));
        txout->tx(shared_from_this());
        txout->txindex(i++);
        txouts_.push_back(txout);
    }

    locktime_ = coin_tx.lockTime;
}

inline unsigned int Tx::missingSigCount() const
{
    // Assume for now all inputs belong to the same account.
    using namespace CoinQ::Script;
    unsigned int count = 0;
    for (auto& txin: txins())
    {
        Script script(txin->script());
        unsigned int sigsneeded = script.sigsneeded();
        if (sigsneeded > count) count = sigsneeded;
    }
    return count;
}

inline std::set<bytes_t> Tx::missingSigPubkeys() const
{
    using namespace CoinQ::Script;
    std::set<bytes_t> pubkeys;
    for (auto& txin: txins())
    {
        Script script(txin->script());
        std::vector<bytes_t> txinpubkeys = script.missingsigs();
        for (auto& txinpubkey: txinpubkeys) { pubkeys.insert(txinpubkey); }
    } 
    return pubkeys;}
}

#endif // COINDB_SCHEMA_HXX
