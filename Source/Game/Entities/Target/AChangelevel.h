#pragma once

#include "BaseEntity.h"

namespace Freeking::Entity::Target
{
    class AChangelevel : public BaseEntity
    {
    public:

        AChangelevel();

		virtual void Initialize() override;
		virtual void Tick(double dt) override;

		const std::string& GetMapName() const { return _mapName; }

	protected:

		virtual bool SetProperty(const EntityProperty& property) override;
		virtual void OnTrigger() override;

    private:

		std::string _mapName;
    };
}
