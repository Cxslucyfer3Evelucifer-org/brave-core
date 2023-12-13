/* Copyright (c) 2023 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/misc_metrics/profile_misc_metrics_service.h"

#include "brave/browser/brave_browser_process.h"
#include "brave/components/misc_metrics/language_metrics.h"
#include "brave/components/misc_metrics/page_metrics.h"
#include "chrome/browser/browser_process.h"

#if BUILDFLAG(IS_ANDROID)
#include "brave/browser/misc_metrics/misc_android_metrics.h"
#else
#include "brave/browser/misc_metrics/extension_metrics.h"
#endif

namespace misc_metrics {

#if BUILDFLAG(IS_ANDROID)
ProfileMiscMetricsService::ProfileMiscMetricsService(
    PrefService* profile_prefs,
    history::HistoryService* history_service,
    SearchEngineTracker* search_engine_tracker) {
#else
ProfileMiscMetricsService::ProfileMiscMetricsService(
    PrefService* profile_prefs,
    extensions::ExtensionRegistry* extension_registry,
    history::HistoryService* history_service,
    SearchEngineTracker* search_engine_tracker) {
#endif
  auto* local_state = g_browser_process->local_state();
  if (profile_prefs) {
    language_metrics_ = std::make_unique<LanguageMetrics>(profile_prefs);
  }
  if (history_service) {
    page_metrics_ = std::make_unique<PageMetrics>(local_state, history_service);
  }
#if BUILDFLAG(IS_ANDROID)
  misc_android_metrics_ = std::make_unique<MiscAndroidMetrics>(
      g_brave_browser_process->process_misc_metrics(), search_engine_tracker);
#else
  if (extension_registry) {
    extension_metrics_ = std::make_unique<ExtensionMetrics>(extension_registry);
  }
#endif
}

ProfileMiscMetricsService::~ProfileMiscMetricsService() = default;

void ProfileMiscMetricsService::Shutdown() {
#if !BUILDFLAG(IS_ANDROID)
  if (extension_metrics_) {
    extension_metrics_->Shutdown();
  }
#endif
}

PageMetrics* ProfileMiscMetricsService::GetPageMetrics() {
  return page_metrics_.get();
}

#if BUILDFLAG(IS_ANDROID)
MiscAndroidMetrics* ProfileMiscMetricsService::GetMiscAndroidMetrics() {
  return misc_android_metrics_.get();
}
#endif

}  // namespace misc_metrics
