#include "sound.h"

#include <engine/shared/datafile.h>
#include <engine/sound.h>

CEditorSound::CEditorSound(CEditorMap *pMap) :
	CMapObject(pMap)
{
}

CEditorSound::~CEditorSound()
{
	Sound()->UnloadSample(m_SoundId);
	free(m_pData);
	m_pData = nullptr;
}

void CEditorSound::SetData(void *pData, unsigned DataSize)
{
	if(m_pData != pData)
		free(m_pData);
	m_pData = pData;
	m_DataSize = DataSize;
	m_CompressedSize = CDataFileWriter::GetCompressedDataSize(m_DataSize, m_pData);
}
