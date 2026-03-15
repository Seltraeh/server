#include "MigrationManager.hpp"
#include "migrations/CreateDefaultTables.hpp"
#include "migrations/CreateUserUnitsTable.hpp"
#include "migrations/AddStatsToUserUnitsTable.hpp"
#include "migrations/PopulateUnitMstTable.hpp"

#define ADD(x) m_migs.push_back(std::make_shared<Migrations::##x>());

void MigrationManager::Register()
{
    ADD(CreateDefaultTables);
    ADD(CreateUserUnitsTable);
    ADD(AddStatsToUserUnitsTable);
    ADD(PopulateUnitMstTable);
}