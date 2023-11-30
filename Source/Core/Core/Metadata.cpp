#include "Metadata.h"
#include <Common/FileUtil.h>
#include <Core/HW/Memmap.h>
#include <Core/HW/AddressSpace.h>
#include <../minizip/mz_compat.h>
#include <codecvt>
#include "zip.h"
#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include <Core/StateAuxillary.h>
#include "Core/IOS/IOS.h"
#include "Core/System.h"

static tm* matchDateTime;
static int homeTeamPossesionFrameCount;
static int awayTeamPossesionFrameCount;
static std::string playerName = "";
std::vector<const NetPlay::Player*> playerArray;
static NetPlay::PadMappingArray netplayWiimoteMap;

static u16 controllerPort1;
static u16 controllerPort2;
static u16 controllerPort3;
static u16 controllerPort4;
static std::vector<int> controllerVector(4);

static u32 homeCaptainID;
static u32 awayCaptainID;
static u32 homeSidekickTopID;
static u32 awaySidekickTopID;
static u32 homeSidekickBackID;
static u32 awaySidekickBackID;
static u32 homeSidekickBottomID;
static u32 awaySidekickBottomID;

static u32 stadiumID;
static u32 overtimeNotReached;
static u32 timeElapsed;

static u16 homeScore;
static u16 awayScore;

static std::array<u8, 16> md5Hash;

std::string Metadata::getJSONString()
{
  char date_string[100];
  char file_name[100];
  strftime(date_string, 50, "%B %d %Y %OH:%OM:%OS", matchDateTime);
  strftime(file_name, 50, "%B_%d_%Y_%OH_%OM_%OS", matchDateTime);
  std::string convertedDate(file_name);
  std::string file_name_string = "Game_" + convertedDate + ".boo";

  std::stringstream json_stream;
  json_stream << "{" << std::endl;
  json_stream << "  \"File Name\": \"" << file_name_string << "\"," << std::endl;
  json_stream << "  \"Date\": \"" << date_string << "\"," << std::endl;
  std::string md5String = "";
  for (int i = 0; i < md5Hash.size(); i++)
  {
    md5String += std::format("{:x}", md5Hash[i]);
  }
  json_stream << "  \"Game Hash\": \"" << md5String << "\"," << std::endl;

  json_stream << "  \"Home Captain ID\": \"" << homeCaptainID << "\"," << std::endl;
  json_stream << "  \"Home Top Sidekick ID\": \"" << homeSidekickTopID << "\"," << std::endl;
  json_stream << "  \"Home Back Sidekick ID\": \"" << homeSidekickBackID << "\"," << std::endl;
  json_stream << "  \"Home Bottom Sidekick ID\": \"" << homeSidekickBottomID << "\"," << std::endl;
  json_stream << "  \"Away Captain ID\": \"" << awayCaptainID << "\"," << std::endl;
  json_stream << "  \"Away Top Sidekick ID\": \"" << awaySidekickTopID << "\"," << std::endl;
  json_stream << "  \"Away Back Sidekick ID\": \"" << awaySidekickBackID << "\"," << std::endl;
  json_stream << "  \"Away Bottom Sidekick ID\": \"" << awaySidekickBottomID << "\"" << std::endl;
  json_stream << "  \"Stadium ID\": \"" << stadiumID << "\"" << std::endl;

  json_stream << "}" << std::endl;

  return json_stream.str();
}

void Metadata::writeJSON(std::string jsonString, bool callBatch)
{
  std::string improvedReplayPath = File::GetUserPath(D_SPOOKYREPLAYS_IDX) + "output.json";
  File::WriteStringToFile(improvedReplayPath, jsonString);

  if (callBatch)
  {
    char date_string[100];
    strftime(date_string, 50, "%B_%d_%Y_%OH_%OM_%OS", matchDateTime);
    std::string someDate(date_string);
    std::string gameTime = "Game_" + someDate;

    // WinExec(batchPath.c_str(), SW_HIDE);
    //  https://stackoverflow.com/questions/11370908/how-do-i-use-minizip-on-zlib
    std::vector<std::wstring> paths;
    std::string exampleFile1 = File::GetUserPath(D_SPOOKYREPLAYS_IDX) + "output.dtm.sav";
    for (char& c : exampleFile1)
    {
      if (c == '/')
        c = '\\';
    }
    std::string exampleFile2 = File::GetUserPath(D_SPOOKYREPLAYS_IDX) + "output.dtm";
    for (char& c : exampleFile2)
    {
      if (c == '/')
        c = '\\';
    }
    std::string exampleFile3 = File::GetUserPath(D_SPOOKYREPLAYS_IDX) + "output.json";
    for (char& c : exampleFile3)
    {
      if (c == '/')
        c = '\\';
    }
    paths.push_back(std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(exampleFile1));
    paths.push_back(std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(exampleFile2));
    paths.push_back(std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(exampleFile3));
    std::string zipName = File::GetUserPath(D_SPOOKYREPLAYS_IDX) + gameTime + ".boo ";

    zipFile zf = zipOpen(zipName.c_str(), APPEND_STATUS_CREATE);
    if (zf == NULL)
      return;
    bool _return = true;
    for (size_t i = 0; i < paths.size(); i++)
    {
      std::fstream file(paths[i].c_str(), std::ios::binary | std::ios::in);
      if (file.is_open())
      {
        file.seekg(0, std::ios::end);
        long size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<char> buffer(size);
        if (size == 0 || file.read(&buffer[0], size))
        {
          zip_fileinfo zfi = {0};
          std::wstring fileName = paths[i].substr(paths[i].rfind('\\') + 1);

          if (ZIP_OK == zipOpenNewFileInZip(zf,
                                            std::string(fileName.begin(), fileName.end()).c_str(),
                                            &zfi, NULL, 0, NULL, 0, NULL, Z_DEFLATED, -1))
          {
            if (zipWriteInFileInZip(zf, size == 0 ? "" : &buffer[0], size))
              _return = false;

            if (zipCloseFileInZip(zf))
              _return = false;

            file.close();
            continue;
          }
        }
        file.close();
      }
      _return = false;
    }

    // common function to delete residual files
    StateAuxillary::endPlayback();
    if (zipClose(zf, NULL))
      return;

    if (!_return)
      return;
    return;
  }
}

void Metadata::setMatchMetadata(tm* matchDateTimeParam)
{
  // have consistent time across the output file and the in-json time
  matchDateTime = matchDateTimeParam;

  // this is the new way to get data in mem, completely untested
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();

  // not working, not sure why
  homeCaptainID = memory.Read_U32(addressHomeCaptainID);
  awayCaptainID = memory.Read_U32(addressAwayCaptainID);
  homeSidekickTopID = memory.Read_U32(addressHomeSidekickTopID);
  awaySidekickTopID = memory.Read_U32(addressAwaySidekickTopID);
  homeSidekickBackID = memory.Read_U32(addressHomeSidekickBackID);
  awaySidekickBackID = memory.Read_U32(addressAwaySidekickBackID);
  homeSidekickBottomID = memory.Read_U32(addressHomeSidekickBottomID);
  awaySidekickBottomID = memory.Read_U32(addressAwaySidekickBottomID);
  stadiumID = memory.Read_U32(addressStadiumID);
}

void Metadata::setPlayerName(std::string playerNameParam)
{
  playerName = playerNameParam;
}

void Metadata::setPlayerArray(std::vector<const NetPlay::Player*> playerArrayParam)
{
  playerArray = playerArrayParam;
  // statViewerPlayers = playerArrayParam;
}

void Metadata::setNetPlayControllers(NetPlay::PadMappingArray m_wiimote_map)
{
  netplayWiimoteMap = m_wiimote_map;
  // statViewerControllers = m_pad_map;
}

void Metadata::setMD5(std::array<u8, 16> md5Param)
{
  md5Hash = md5Param;
}

std::vector<const NetPlay::Player*> Metadata::getPlayerArray()
{
  return playerArray;
}

NetPlay::PadMappingArray Metadata::getControllers()
{
  return netplayWiimoteMap;
}
