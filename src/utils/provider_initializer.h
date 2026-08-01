#pragma once

#include "providers/provider_manager.h"

// Registers every available provider onto the given ProviderManager and
// selects the default model.
void initializeProviders(ProviderManager& provider);
