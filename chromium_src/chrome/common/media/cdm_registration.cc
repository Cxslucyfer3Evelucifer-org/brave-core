/* Copyright (c) 2023 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "chrome/common/media/cdm_registration.h"

#include "third_party/widevine/cdm/buildflags.h"

#if BUILDFLAG_INTERNAL_BUNDLE_WIDEVINE_CDM() == 1
#define BUNDLE_WIDEVINE_WAS_SET
#endif

#undef BUILDFLAG_INTERNAL_BUNDLE_WIDEVINE_CDM
#define BUILDFLAG_INTERNAL_BUNDLE_WIDEVINE_CDM \
  BUILDFLAG_INTERNAL_ENABLE_WIDEVINE_CDM_COMPONENT
#include "src/chrome/common/media/cdm_registration.cc"
#undef BUILDFLAG_INTERNAL_BUNDLE_WIDEVINE_CDM

#ifdef BUNDLE_WIDEVINE_WAS_SET
#define BUILDFLAG_INTERNAL_BUNDLE_WIDEVINE_CDM() (1)
#else
#define BUILDFLAG_INTERNAL_BUNDLE_WIDEVINE_CDM() (0)
#endif
#undef BUNDLE_WIDEVINE_WAS_SET
