/* Copyright (c) 2023 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_rewards/core/wallet_provider/solana/solana_wallet_provider.h"

#include <cmath>
#include <utility>
#include <vector>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "brave/components/brave_rewards/core/common/security_util.h"
#include "brave/components/brave_rewards/core/endpoint/rewards/rewards_util.h"
#include "brave/components/brave_rewards/core/endpoints/request_for.h"
#include "brave/components/brave_rewards/core/global_constants.h"
#include "brave/components/brave_rewards/core/logging/event_log_keys.h"
#include "brave/components/brave_rewards/core/rewards_engine_impl.h"
#include "brave/components/brave_rewards/core/state/state_keys.h"
#include "brave/components/brave_rewards/core/wallet/wallet_util.h"
#include "brave/components/brave_rewards/core/wallet_provider/linkage_checker.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace brave_rewards::internal {

namespace {

constexpr char kSplBatTokenMint[] =
    "EPeUFDgHRxs9xxEPVaL6kfGQvCon7jmAWKVUHuux1Tpz";
constexpr base::TimeDelta kPollingInterval = base::Seconds(10);
constexpr base::TimeDelta kPollingTimeout = base::Minutes(5);

std::string UsernameFromAddress(const std::string& address) {
  size_t length = address.length();
  if (length < 10) {
    return address;
  }
  std::string_view address_view(address);
  return base::StrCat({address_view.substr(0, 5), "...",
                       address_view.substr(length - 4, length)});
}

}  // namespace

SolanaWalletProvider::SolanaWalletProvider(RewardsEngineImpl& engine)
    : WalletProvider(engine), engine_(engine) {}

SolanaWalletProvider::~SolanaWalletProvider() = default;

const char* SolanaWalletProvider::WalletType() const {
  return constant::kWalletSolana;
}

void SolanaWalletProvider::AssignWalletLinks(
    mojom::ExternalWallet& external_wallet) {
  std::string explorer_url =
      base::StrCat({"https://solscan.io/account/", external_wallet.address});
  external_wallet.account_url = explorer_url;
  external_wallet.activity_url = explorer_url;
}

void SolanaWalletProvider::FetchBalance(
    base::OnceCallback<void(mojom::Result, double)> callback) {
  auto wallet = GetWalletIf({mojom::WalletStatus::kConnected});
  if (!wallet) {
    return std::move(callback).Run(mojom::Result::FAILED, 0.0);
  }

  engine_->client()->GetSPLTokenAccountBalance(
      wallet->address, kSplBatTokenMint,
      base::BindOnce(&SolanaWalletProvider::GetAccountBalanceCallback,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void SolanaWalletProvider::BeginLogin(
    BeginExternalWalletLoginCallback callback) {
  endpoints::RequestFor<endpoints::PostChallenges>(*engine_).Send(
      base::BindOnce(&SolanaWalletProvider::PostChallengesCallback,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void SolanaWalletProvider::PostChallengesCallback(
    BeginExternalWalletLoginCallback callback,
    endpoints::PostChallenges::Result&& result) {
  if (!result.has_value()) {
    std::move(callback).Run(nullptr);
    return;
  }

  const std::string& challenge_id = result.value();
  DCHECK(!challenge_id.empty());

  const auto wallet = engine_->wallet()->GetWallet();
  if (!wallet) {
    BLOG(0, "Rewards wallet is empty");
    std::move(callback).Run(nullptr);
    return;
  }

  std::string message = base::StrCat({wallet->payment_id, ".", challenge_id});

  auto signed_message = util::Security::SignMessageWithSeed(
      std::vector<uint8_t>(message.begin(), message.end()),
      wallet->recovery_seed);
  if (!signed_message) {
    BLOG(0, "Unable to sign message with seed");
    std::move(callback).Run(nullptr);
    return;
  }

  std::string signature = util::Security::Uint8ToHex(*signed_message);

  GURL url(endpoint::rewards::GetServerUrl("/connect"));
  url = net::AppendOrReplaceQueryParameter(url, "msg", message);
  url = net::AppendOrReplaceQueryParameter(url, "sig", signature);

  auto params = mojom::ExternalWalletLoginParams::New();
  params->url = url.spec();
  params->cookies["__Secure-CSRF_TOKEN"] = challenge_id;
  std::move(callback).Run(std::move(params));

  polling_timer_.Start(FROM_HERE, kPollingInterval, this,
                       &SolanaWalletProvider::PollWalletStatus);

  polling_timeout_.Start(FROM_HERE, kPollingTimeout, this,
                         &SolanaWalletProvider::OnPollingTimeout);
}

void SolanaWalletProvider::GetAccountBalanceCallback(
    base::OnceCallback<void(mojom::Result, double)> callback,
    mojom::SolanaAccountBalancePtr balance) {
  if (!balance) {
    BLOG(0, "Unable to retrieve Solana account balance");
    std::move(callback).Run(mojom::Result::FAILED, 0);
    return;
  }

  uint64_t amount_value = 0;
  if (!base::StringToUint64(balance->amount, &amount_value)) {
    BLOG(0, "Unable to parse Solana account balance");
    std::move(callback).Run(mojom::Result::FAILED, 0);
    return;
  }

  auto denominator = std::pow<double>(10, balance->decimals);

  std::move(callback).Run(mojom::Result::OK,
                          static_cast<double>(amount_value) / denominator);
}

void SolanaWalletProvider::PollWalletStatus() {
  engine_->Get<LinkageChecker>().CheckLinkage();
}

void SolanaWalletProvider::OnPollingTimeout() {
  polling_timer_.Stop();
}

std::string SolanaWalletProvider::GetFeeAddress() const {
  return "";
}

void SolanaWalletProvider::OnWalletLinked(const std::string& address) {
  DCHECK(!address.empty());

  polling_timer_.Stop();
  polling_timeout_.Stop();

  wallet::MaybeCreateWallet(*engine_, WalletType());
  auto wallet = GetWalletIf({mojom::WalletStatus::kNotConnected});
  if (!wallet) {
    return;
  }

  // Current external wallet invariants require both an address and a token for
  // connected wallets. Use an arbitrary non-empty value for the token.
  wallet->address = address;
  wallet->token = address;
  wallet->user_name = UsernameFromAddress(address);

  if (!wallet::TransitionWallet(*engine_, std::move(wallet),
                                mojom::WalletStatus::kConnected)) {
    BLOG(0, "Failed to transition " << WalletType() << " wallet state");
    return;
  }

  engine_->SetState(state::kExternalWalletType, std::string(WalletType()));
  engine_->client()->ExternalWalletConnected();
  engine_->database()->SaveEventLog(
      log::kWalletVerified,
      WalletType() + std::string("/") + address.substr(0, 5));
}

}  // namespace brave_rewards::internal
