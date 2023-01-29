#include <zephyr/logging/log.h>
#include <zephyr/net/openthread.h>

#include <openthread/srp_client.h>

LOG_MODULE_REGISTER(srp_init);

otSrpClientService threadNodeService = {
    .mName = "_threadnode._udp",
    .mPort = 1234,
    .mInstanceName = "Thread Node",
    .mSubTypeLabels = NULL,
    .mTxtEntries = NULL,
    .mNumTxtEntries = 0,
    .mPriority = 0,
    .mWeight = 0,
};

int srp_init() {
  otError error;

  otInstance *ot = openthread_get_default_instance();
  if (!ot) {
    LOG_ERR("%s", "Failed to get openthread default instance");
    return 1;
  }

  otSrpClientEnableAutoStartMode(ot, NULL, NULL);

  error = otSrpClientSetHostName(ot, "garage");
  if (error != OT_ERROR_NONE) {
    LOG_ERR("Failed to set SRP hostname. Error: %d", error);
    return 1;
  }

  error = otSrpClientEnableAutoHostAddress(ot);
  if (error != OT_ERROR_NONE) {
    LOG_ERR("Failed to set SRP addresses. Error: %d", error);
    return 1;
  }

  error = otSrpClientAddService(ot, &threadNodeService);
  if (error != OT_ERROR_NONE) {
    LOG_ERR("Failed to add Service. Error: %d", error);
    return 1;
  }

  LOG_INF("%s", "Started SRP");

  return 0;
}

SYS_INIT(srp_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
