/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_SNAPSHOT_H
#define ENGINE_SHARED_SNAPSHOT_H

#include <cstddef>
#include <cstdint>
#include <optional>

// CSnapshot

class CSnapshotItem
{
	friend class CSnapshotBuilder;

	int *Data() { return (int *)(this + 1); }

public:
	int m_TypeAndId;

	const int *Data() const { return (int *)(this + 1); }
	int InternalType() const { return m_TypeAndId >> 16; }
	int Id() const { return m_TypeAndId & 0xffff; }
	int Key() const { return m_TypeAndId; }
	void Invalidate() { m_TypeAndId = -1; }
};

class CSnapshot
{
	friend class CSnapshotBuilder;
	int m_DataSize = 0;
	int m_NumItems = 0;

	int *Offsets() const { return (int *)(this + 1); }
	char *DataStart() const { return (char *)(Offsets() + m_NumItems); }

	size_t OffsetSize() const { return sizeof(int) * m_NumItems; }
	size_t TotalSize() const { return sizeof(CSnapshot) + OffsetSize() + m_DataSize; }

	static const CSnapshot ms_EmptySnapshot;

public:
	enum
	{
		OFFSET_UUID_TYPE = 0x4000,
		MAX_TYPE = 0x7fff,
		MAX_ID = 0xffff,
		MAX_ITEMS = 1024,
		MAX_PARTS = 64,
		MAX_SIZE = MAX_PARTS * 1024
	};

	int NumItems() const { return m_NumItems; }
	int DataSize() const { return m_DataSize; }
	const CSnapshotItem *GetItem(int Index) const;
	int GetItemSize(int Index) const;
	int GetItemIndex(int Key) const;
	void InvalidateItem(int Index);
	int GetItemType(int Index) const;
	int GetExternalItemType(int InternalType) const;
	const void *FindItem(int Type, int Id) const;

	unsigned Crc() const;
	// Prints the raw snapshot data showing item and int boundaries.
	// See also `CNetObjHandler::DebugDumpSnapshot(const CSnapshot *pSnap)`
	// For more detailed annotations of the data.
	void DebugDump() const;
	bool IsValid(size_t ActualSize) const;

	static const CSnapshot *EmptySnapshot() { return &ms_EmptySnapshot; }
};

class alignas(int32_t) CSnapshotBuffer
{
public:
	unsigned char m_aData[CSnapshot::MAX_SIZE];

	CSnapshot *AsSnapshot() { return (CSnapshot *)m_aData; }
	const CSnapshot *AsSnapshot() const { return (const CSnapshot *)m_aData; }
};

class alignas(int32_t) CSnapshotDeltaBuffer
{
	// A delta always fits into twice the snapshot size.
	static constexpr int MAX_SIZE = 2 * CSnapshot::MAX_SIZE;

public:
	unsigned char m_aData[MAX_SIZE];
};

// CSnapshotDelta

class CSnapshotDelta
{
public:
	class CData
	{
	public:
		int m_NumDeletedItems;
		int m_NumUpdateItems;
		int m_NumTempItems; // needed?
		int m_aData[1];
	};

private:
	enum
	{
		MAX_NETOBJSIZES = 64
	};
	short m_aItemSizes[MAX_NETOBJSIZES];
	uint64_t m_aSnapshotDataRate[CSnapshot::MAX_TYPE + 1];
	uint64_t m_aSnapshotDataUpdates[CSnapshot::MAX_TYPE + 1];
	CData m_Empty;

	static void UndiffItem(const int *pPast, const int *pDiff, int *pOut, int Size, uint64_t *pDataRate);

public:
	static int DiffItem(const int *pPast, const int *pCurrent, int *pOut, int Size);
	CSnapshotDelta();
	CSnapshotDelta(const CSnapshotDelta &Old);
	uint64_t GetDataRate(int Index) const { return m_aSnapshotDataRate[Index]; }
	uint64_t GetDataUpdates(int Index) const { return m_aSnapshotDataUpdates[Index]; }
	void SetStaticsize(int ItemType, size_t Size);
	const CData *EmptyDelta() const;
	int CreateDelta(const CSnapshot *pFrom, const CSnapshot *pTo, CSnapshotDeltaBuffer *pDstData);
	int UnpackDelta(const CSnapshot *pFrom, CSnapshotBuffer *pTo, const void *pSrcData, int DataSize);
	int DebugDumpDelta(const void *pSrcData, int DataSize);
};

// CSnapshotStorage

// EClient: resolving the type of an extended (uuid) item means finding its
// NETOBJTYPE_EX declaration item by a linear scan and then looking the uuid up, which
// `CSnapshot` does again for every single item of every sweep over a snapshot. On a busy
// server roughly a third of the items are extended and the snapshot is walked several
// times per tick, which measured ~100us per sweep. This resolves every declaration of
// one snapshot in a single pass and answers from the table afterwards. The results are
// identical to `CSnapshot::GetItemType`, including -1 for a missing or unknown type.
class CSnapshotItemTypes
{
	// Only the types a `CSnapshotBuilder` can emit are cached, anything else falls back
	// to the uncached path.
	enum
	{
		NUM_TYPES = 64,
	};

	const CSnapshot *m_pSnapshot;
	int m_aExternalTypes[NUM_TYPES];

	void Build(const CSnapshot *pSnapshot);
	static int CacheIndex(int InternalType);

public:
	// Must be called before first use and whenever the snapshot's contents change. Holders
	// are allocated without running constructors, so this is not a constructor.
	void Invalidate() { m_pSnapshot = nullptr; }

	int ItemType(const CSnapshot *pSnapshot, int Index);
	int InternalItemType(const CSnapshot *pSnapshot, int ExternalType);
};

// EClient: key -> item index for one snapshot. `CSnapshot::GetItemIndex` is a linear
// scan ("this should not be a linear search. very bad") and the client runs one per
// player and per character of every snapshot, so the cost grows with the square of the
// player count. Lookups answer from this table instead, which reports the same index as
// the scan, the first one when a key occurs twice.
class CSnapshotItemIndex
{
	enum
	{
		// power of two, kept well above CSnapshot::MAX_ITEMS so probes stay short
		CAPACITY = 2048,
	};

	const CSnapshot *m_pSnapshot;
	int m_aItemIndices[CAPACITY];

	void Build(const CSnapshot *pSnapshot);
	static int Slot(int Key);

public:
	// Must be called before first use and whenever the snapshot's contents change.
	void Invalidate() { m_pSnapshot = nullptr; }

	int ItemIndex(const CSnapshot *pSnapshot, int Key);
};

class CSnapshotStorage
{
public:
	class CHolder
	{
	public:
		CHolder *m_pPrev;
		CHolder *m_pNext;

		int64_t m_Tagtime;
		int m_Tick;

		int m_SnapSize;
		int m_AltSnapSize;

		CSnapshot *m_pSnap;
		CSnapshot *m_pAltSnap;
		CSnapshotItemTypes m_AltSnapTypes; // EClient
		CSnapshotItemIndex m_AltSnapIndex; // EClient
	};

	CHolder *m_pFirst;
	CHolder *m_pLast;

	CSnapshotStorage() { Init(); }
	~CSnapshotStorage() { PurgeAll(); }
	void Init();
	void PurgeAll();
	void PurgeUntil(int Tick);
	void Add(int Tick, int64_t Tagtime, size_t DataSize, const void *pData, size_t AltDataSize, const void *pAltData);
	int Get(int Tick, int64_t *pTagtime, const CSnapshot **ppData, const CSnapshot **ppAltData) const;
};

class CSnapshotBuilder
{
	enum
	{
		MAX_EXTENDED_ITEM_TYPES = 64,
	};

	char m_aData[CSnapshot::MAX_SIZE];
	int m_DataSize;

	int m_aOffsets[CSnapshot::MAX_ITEMS];
	int m_NumItems;

	// EClient: key -> item index for the items added so far, so that repeated
	// `FindItemIndexByKey` calls do not each scan the whole builder. Built on the first
	// lookup and maintained from there, which keeps it off the path of users that only
	// ever add items, like the server building its snapshots.
	enum
	{
		ITEM_INDEX_CAPACITY = 2048,
	};
	int m_aItemIndices[ITEM_INDEX_CAPACITY];
	bool m_ItemIndexBuilt = false;
	void BuildItemIndex();
	void AddToItemIndex(int Key, int ItemIndex);

	int m_aExtendedItemTypes[MAX_EXTENDED_ITEM_TYPES];
	int m_NumExtendedItemTypes = 0;

	bool AddExtendedItemType(int Index);
	int GetExtendedItemTypeIndex(int TypeId);
	int GetTypeFromIndex(int Index) const;

	bool m_Building = false;
	bool m_HasDroppedItem = false;
	bool m_Sixup = false;

public:
	void Init(bool Sixup = false);
	void Init7(const CSnapshot *pSnapshot);

	bool NewItem(int Type, int Id, const void *pData, int Size);
	void *NewItemRaw(int Type, int Id, int Size);

	CSnapshotItem *GetItem(int Index);
	int GetItemSize(int Index) const;
	int *GetItemData(int Index);
	std::optional<int> FindItemIndexByKey(int Key);

	int FinishIfNoDroppedItems(CSnapshotBuffer *pSnapData);
	int Finish(CSnapshotBuffer *pBuffer);
};

#endif // ENGINE_SHARED_SNAPSHOT_H
