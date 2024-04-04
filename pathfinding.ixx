#include "framework.h"

export module pathfinding;

constexpr int MAX_POLYS = 1024;
constexpr int NAVMESHSET_MAGIC = 'M' << 24 | 'S' << 16 | 'E' << 8 | 'T'; // 'MSET'
constexpr int NAVMESHSET_VERSION = 1;

constexpr float MIN_DISTANCE = 1.05f;

export dtNavMesh* mesh{};
export dtNavMeshQuery* query{};
dtPolyRef poly_path[MAX_POLYS]{};

enum PolyFlags
{
	POLYFLAGS_WALK = 0x01,
	POLYFLAGS_SWIM = 0x02,
	POLYFLAGS_DOOR = 0x04,
	POLYFLAGS_JUMP = 0x08,
	POLYFLAGS_DISABLED = 0x10,
	POLYFLAGS_ALL = 0xffff
};

struct NavMeshSetHeader
{
	int magic;
	int version;
	int numTiles;
	dtNavMeshParams params;
};

struct NavMeshTileHeader
{
	dtTileRef tileRef;
	int dataSize;
};

export dtNavMesh* load_tiles(const char* path)
{
	FILE* fp = nullptr;

	if (fopen_s(&fp, path, "rb") != 0) {
		printf("Error opening file\n");

		return 0;
	}

	NavMeshSetHeader header;
	size_t readLen = fread(&header, sizeof(NavMeshSetHeader), 1, fp);
	if (readLen != 1)
	{
		fclose(fp);
		return 0;
	}
	if (header.magic != NAVMESHSET_MAGIC)
	{
		fclose(fp);
		return 0;
	}
	if (header.version != NAVMESHSET_VERSION)
	{
		fclose(fp);
		return 0;
	}

	dtNavMesh* mesh = dtAllocNavMesh();
	if (!mesh)
	{
		fclose(fp);
		return 0;
	}
	dtStatus status = mesh->init(&header.params);
	if (dtStatusFailed(status))
	{
		fclose(fp);
		return 0;
	}

	for (int i = 0; i < header.numTiles; ++i)
	{
		NavMeshTileHeader tileHeader;
		readLen = fread(&tileHeader, sizeof(tileHeader), 1, fp);
		if (readLen != 1)
		{
			fclose(fp);
			return 0;
		}

		if (!tileHeader.tileRef || !tileHeader.dataSize)
			break;

		unsigned char* data = (unsigned char*)dtAlloc(tileHeader.dataSize, DT_ALLOC_PERM);
		if (!data) break;
		memset(data, 0, tileHeader.dataSize);
		readLen = fread(data, tileHeader.dataSize, 1, fp);
		if (readLen != 1)
		{
			dtFree(data);
			fclose(fp);
			return 0;
		}

		mesh->addTile(data, tileHeader.dataSize, DT_TILE_FREE_DATA, tileHeader.tileRef, 0);
	}

	fclose(fp);

	return mesh;
}

export void coords_rd(float* pos) noexcept
{
	float ox = pos[0];
	pos[0] = pos[1];
	pos[1] = pos[2];
	pos[2] = ox;
}
export void coords_wow(float* pos) noexcept
{
	float oz = pos[2];
	pos[2] = pos[1];
	pos[1] = pos[0];
	pos[0] = oz;
}

export std::vector<vector3> find_path(float* data, float* path, int* sz_path)
{
	auto start = &data[0];
	auto end = &data[3];

	float rd_start[3]{};
	dtVcopy(rd_start, start);
	coords_rd(rd_start);

	float rd_end[3]{};
	dtVcopy(rd_end, end);
	coords_rd(rd_end);

	float extents[3]{ 6.0f, 6.0f, 6.0f };

	dtQueryFilter filter;
	dtPolyRef start_poly, end_poly;

	filter.setIncludeFlags(POLYFLAGS_WALK);
	filter.setExcludeFlags(0);

	query->findNearestPoly(rd_start, extents, &filter, &start_poly, rd_start);
	query->findNearestPoly(rd_end, extents, &filter, &end_poly, rd_end);

	auto status = query->findPath(start_poly, end_poly, rd_start, rd_end, &filter, poly_path, sz_path, MAX_POLYS);

	std::vector<vector3> results{};

	if (dtStatusSucceed(status))
	{
		dtStatus straight_status = query->findStraightPath
		(
			rd_start,
			rd_end,
			poly_path,
			*sz_path,
			path,
			nullptr,
			nullptr,
			sz_path,
			MAX_POLYS
		);

		if (dtStatusSucceed(straight_status))
		{
			for (int i = 0; i < *sz_path * 3; i += 3)
			{
				auto waypoint_rd = &path[i];
				float waypoint[3];
				dtVcopy(waypoint, waypoint_rd);
				coords_wow(waypoint);

				results.emplace_back(waypoint);
			}
		}
	}

	return results;
}

export std::vector<vector3> calculate_path(float* data) {
	float* path = new float[MAX_POLYS];
	int sz_path = 0;

	auto results = find_path(data, path, &sz_path);

	printf("Path Size: %lli\n", results.size());

	delete[] path;

	return results;
}

export int load_mesh()
{
	std::filesystem::path source_path(__FILE__);

	// WARNING: Make sure it's production ready IO
	auto mesh_name = "all_tiles_navmesh.bin";
	auto parent_path = source_path.parent_path().parent_path().append("maps_retail\\").string();
	auto mesh_path = parent_path + mesh_name;

	//std::cout << "Loaded Mesh: " << mesh_path << std::endl;

	mesh = load_tiles(mesh_path.c_str());

	if (!mesh) {
		std::cerr << "Failed to initalize mesh" << std::endl;
		return -1;
	}

	query = dtAllocNavMeshQuery();

	if (!query) {
		std::cerr << "Failed to allocate mesh query!" << std::endl;

		return -1;
	}

	if (query->init(mesh, MAX_POLYS) != DT_SUCCESS) {
		std::cerr << "Failed to init mesh!" << std::endl;

		dtFree(query);
		return -1;
	}

	return 1;
}