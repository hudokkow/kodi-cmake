/*
 *      Copyright (C) 2005-2013 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "network/Network.h"
#include "system.h"
#include "utils/AlarmClock.h"
#include "utils/Screenshot.h"
#include "utils/SeekHandler.h"
#include "Application.h"
#include "messaging/ApplicationMessenger.h"
#include "Autorun.h"
#include "Builtins.h"
#include "input/ButtonTranslator.h"
#include "input/InputManager.h"
#include "FileItem.h"
#include "addons/GUIDialogAddonSettings.h"
#include "dialogs/GUIDialogFileBrowser.h"
#include "guilib/GUIKeyboardFactory.h"
#include "input/Key.h"
#include "guilib/StereoscopicsManager.h"
#include "guilib/GUIAudioManager.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "dialogs/GUIDialogNumeric.h"
#include "dialogs/GUIDialogProgress.h"
#include "dialogs/GUIDialogYesNo.h"
#include "GUIUserMessages.h"
#include "windows/GUIWindowLoginScreen.h"
#include "video/windows/GUIWindowVideoBase.h"
#include "addons/GUIWindowAddonBrowser.h"
#include "addons/Addon.h" // for TranslateType, TranslateContent
#include "addons/AddonInstaller.h"
#include "addons/AddonManager.h"
#include "addons/PluginSource.h"
#include "addons/Skin.h"
#include "interfaces/generic/ScriptInvocationManager.h"
#include "interfaces/AnnouncementManager.h"
#include "network/NetworkServices.h"
#include "utils/log.h"
#include "storage/MediaManager.h"
#include "utils/RssManager.h"
#include "utils/JSONVariantParser.h"
#include "PartyModeManager.h"
#include "profiles/ProfilesManager.h"
#include "settings/DisplaySettings.h"
#include "settings/Settings.h"
#include "settings/MediaSettings.h"
#include "settings/MediaSourceSettings.h"
#include "settings/SkinSettings.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/Variant.h"
#include "video/VideoLibraryQueue.h"
#include "Util.h"
#include "URL.h"
#include "music/MusicDatabase.h"
#include "cores/IPlayer.h"
#include "pvr/channels/PVRChannel.h"
#include "pvr/recordings/PVRRecording.h"
#include "pvr/PVRManager.h"

#include "filesystem/PluginDirectory.h"
#include "filesystem/ZipManager.h"

#include "guilib/GUIWindowManager.h"
#include "guilib/LocalizeStrings.h"

#ifdef HAS_LIRC
#include "input/linux/LIRC.h"
#endif
#ifdef HAS_IRSERVERSUITE

  #include "input/windows/IRServerSuite.h"

#endif

#if defined(TARGET_DARWIN)
#include "filesystem/SpecialProtocol.h"
#include "osx/CocoaInterface.h"
#endif

#ifdef HAS_CDDA_RIPPER
#include "cdrip/CDDARipper.h"
#endif

#include <vector>
#include "settings/AdvancedSettings.h"
#include "settings/DisplaySettings.h"
#include "powermanagement/PowerManager.h"
#include "filesystem/Directory.h"

#include "AddonBuiltins.h"
#include "ApplicationBuiltins.h"
#include "CECBuiltins.h"
#include "GUIBuiltins.h"
#include "GUIControlBuiltins.h"
#include "GUIContainerBuiltins.h"
#include "LibraryBuiltins.h"
#include "OpticalBuiltins.h"
#include "PictureBuiltins.h"
#include "PlayerBuiltins.h"
#include "ProfileBuiltins.h"
#include "PVRBuiltins.h"
#include "SkinBuiltins.h"
#include "SystemBuiltins.h"
#include "WeatherBuiltins.h"

#if defined(TARGET_ANDROID)
#include "AndroidBuiltins.h"
#endif

using namespace std;
using namespace XFILE;
using namespace ADDON;
using namespace KODI::MESSAGING;

#ifdef HAS_DVD_DRIVE
using namespace MEDIA_DETECT;
#endif

typedef struct
{
  const char* command;
  bool needsParameters;
  const char* description;
} BUILT_IN;

const BUILT_IN commands[] = {
  { "Help",                       false,  "This help message" },
};

CBuiltins::CBuiltins()
{
  RegisterCommands<CAddonBuiltins>();
  RegisterCommands<CApplicationBuiltins>();
  RegisterCommands<CGUIBuiltins>();
  RegisterCommands<CGUIContainerBuiltins>();
  RegisterCommands<CGUIControlBuiltins>();
  RegisterCommands<CLibraryBuiltins>();
  RegisterCommands<COpticalBuiltins>();
  RegisterCommands<CPictureBuiltins>();
  RegisterCommands<CPlayerBuiltins>();
  RegisterCommands<CProfileBuiltins>();
  RegisterCommands<CPVRBuiltins>();
  RegisterCommands<CSkinBuiltins>();
  RegisterCommands<CSystemBuiltins>();
  RegisterCommands<CWeatherBuiltins>();

#if defined(TARGET_ANDROID)
  RegisterCommands<CAndroidBuiltins>();
#endif
}

CBuiltins::~CBuiltins()
{
}

CBuiltins& CBuiltins::Get()
{
  static CBuiltins sBuiltins;
  return sBuiltins;
}

bool CBuiltins::HasCommand(const std::string& execString)
{
  std::string function;
  std::vector<string> parameters;
  CUtil::SplitExecFunction(execString, function, parameters);
  StringUtils::ToLower(function);
  const auto& it = m_command.find(function);
  if (it != m_command.end())
  {
    if (it->second.parameters == 0 || it->second.parameters <= parameters.size())
      return true;
  }
  else
  {
    for (unsigned int i = 0; i < sizeof(commands)/sizeof(BUILT_IN); i++)
    {
      if (StringUtils::EqualsNoCase(function, commands[i].command) && (!commands[i].needsParameters || parameters.size()))
        return true;
    }
  }

  return false;
}

bool CBuiltins::IsSystemPowerdownCommand(const std::string& execString)
{
  std::string execute;
  vector<string> params;
  CUtil::SplitExecFunction(execString, execute, params);
  StringUtils::ToLower(execute);

  // Check if action is resulting in system powerdown.
  if (execute == "reboot"    ||
      execute == "restart"   ||
      execute == "reset"     ||
      execute == "powerdown" ||
      execute == "hibernate" ||
      execute == "suspend" )
  {
    return true;
  }
  else if (execute == "shutdown")
  {
    switch (CSettings::Get().GetInt(CSettings::SETTING_POWERMANAGEMENT_SHUTDOWNSTATE))
    {
      case POWERSTATE_SHUTDOWN:
      case POWERSTATE_SUSPEND:
      case POWERSTATE_HIBERNATE:
        return true;

      default:
        return false;
    }
  }
  return false;
}

void CBuiltins::GetHelp(std::string &help)
{
  help.clear();

  for (const auto& it : m_command)
  {
    help += it.first;
    help += "\t";
    help += it.second.description;
    help += "\n";
  }

  for (unsigned int i = 0; i < sizeof(commands)/sizeof(BUILT_IN); i++)
  {
    help += commands[i].command;
    help += "\t";
    help += commands[i].description;
    help += "\n";
  }
}

int CBuiltins::Execute(const std::string& execString)
{
  // Deprecated. Get the text after the "XBMC."
  std::string execute;
  vector<string> params;
  CUtil::SplitExecFunction(execString, execute, params);
  StringUtils::ToLower(execute);
  std::string parameter = params.size() ? params[0] : "";
  std::string paramlow(parameter);
  StringUtils::ToLower(paramlow);

  const auto& it = m_command.find(execute);
  if (it != m_command.end())
  {
    if (it->second.parameters == 0 || params.size() >= it->second.parameters)
      return it->second.Execute(params);
    else
    {
      CLog::Log(LOGERROR, "%s called with invalid number of parameters (should be: %" PRIdS ", is %" PRIdS")",
                          execute.c_str(), it->second.parameters, params.size());
      return -1;
    }
  }

  if (execute == "startandroidactivity" && !params.empty())
  {
    CApplicationMessenger::Get().PostMsg(TMSG_START_ANDROID_ACTIVITY, -1, -1, static_cast<void*>(&params));
  }
  else
    return CInputManager::Get().ExecuteBuiltin(execute, params);
  return 0;
}
