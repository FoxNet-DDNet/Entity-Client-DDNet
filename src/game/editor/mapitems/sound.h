#ifndef GAME_EDITOR_MAPITEMS_SOUND_H
#define GAME_EDITOR_MAPITEMS_SOUND_H

#include <base/types.h>

#include <game/editor/map_object.h>

class CEditorSound : public CMapObject
{
public:
	explicit CEditorSound(CEditorMap *pMap);
	~CEditorSound() override;
	// Takes ownership of pData and updates the cached map size.
	void SetData(void *pData, unsigned DataSize);

	int m_SoundId = -1;
	char m_aName[IO_MAX_PATH_LENGTH] = "";

	void *m_pData = nullptr;
	unsigned m_DataSize = 0;
	size_t m_CompressedSize = 0;
};

#endif
