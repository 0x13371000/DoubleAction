#pragma once

#ifdef WITH_DATA_COLLECTION

#include "vstdlib/jobthread.h"

#include "sdk_shareddefs.h"

namespace da
{
	namespace protobuf
	{
		class GameData;
	}
}

class CDataManager : public CAutoGameSystemPerFrame
{
public:
	CDataManager( char const *name );
	virtual ~CDataManager();

public:
	virtual void LevelInitPostEntity();
	virtual void FrameUpdatePostEntityThink();
	virtual void LevelShutdownPostEntity();

	virtual void SavePositions();

	void AddPlayerKill(const Vector& vecPosition);
	void AddPlayerDeath(const Vector& vecPosition);

	void AddCharacterChosen(const char* pszCharacter);
	void AddWeaponChosen(SDKWeaponID eWeapon);
	void AddSkillChosen(SkillID eSkill);

	void ClientConnected(AccountID_t eAccountID);
	void ClientDisconnected(AccountID_t eAccountID);

	void SetTeamplay(bool bOn);

	void VotePassed(const char* pszIssue, const char* pszDetails);
	void VoteFailed(const char* pszIssue);

	bool IsSendingData();

	void FillProtoBuffer(da::protobuf::GameData* pbGameData);
	void ClearData();

private:
	class CDataContainer
	{
	private:
		static bool Str_LessFunc( CUtlString const &a, CUtlString const &b )
		{
			return strcmp(a.Get(), b.Get()) < 0;
		}

	public:
		CDataContainer()
		{
			m_flStartTime = 0;

			m_flNextPositionsUpdate = 0;

			m_iConnections = 0;
			m_iDisconnections = 0;
			m_iUniquePlayers = 0;

			m_iThirdPersonActive = 0;
			m_iThirdPersonInactive = 0;

			m_bCheated = false;

			m_asCharactersChosen.SetLessFunc(Str_LessFunc);
		}

		float              m_flStartTime;
		bool               m_bTeamplay;
		float              m_flNextPositionsUpdate;
		CUtlVector<Vector> m_avecPlayerPositions;
		CUtlVector<Vector> m_avecPlayerKills;
		CUtlVector<Vector> m_avecPlayerDeaths;
		CUtlMap<CUtlString, int> m_asCharactersChosen;
		CUtlVector<SDKWeaponID> m_aeWeaponsChosen;
		CUtlVector<SkillID>     m_aeSkillsChosen;
		int                m_iConnections;
		int                m_iDisconnections;
		int                m_iUniquePlayers;
		int                m_iThirdPersonActive;
		int                m_iThirdPersonInactive;

		struct VoteResult
		{
			CUtlString  m_sIssue;
			CUtlString  m_sDetails;
			bool        m_bResult;
		};
		CUtlVector<VoteResult> m_aVoteResults;

		bool               m_bCheated;
	}* d;

	// We use this to figure out if a client has really connected
	// so it needs to be outside the data container so it doesn't get wiped.
	CUtlMap<AccountID_t, char> m_aiConnectedClients;

	bool  m_bLevelStarted;
	CJob* m_pSendData;
};

CDataManager& DataManager();

#endif
