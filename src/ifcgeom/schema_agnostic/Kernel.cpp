#include "Kernel.h"

#include "../../ifcparse/Ifc2x3.h"
#include "../../ifcparse/Ifc4.h"

// @todo remove
#include "../../ifcgeom/schema_agnostic/opencascade/OpenCascadeConversionResult.h"

#include <TopExp.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>

IfcGeom::Kernel::Kernel(const std::string& geometry_library, IfcParse::IfcFile* file) {
	if (file != 0) {
		if (file->schema() == 0) {
			throw IfcParse::IfcException("No schema associated with file");
		}

		const std::string& schema_name = file->schema()->name();
		implementation_ = impl::kernel_implementations().construct(schema_name, geometry_library, file);
	}
}

int IfcGeom::Kernel::count(const ConversionResultShape* s_, int t_, bool unique) {
	// @todo make kernel agnostic
	const TopoDS_Shape& s = ((OpenCascadeShape*) s_)->shape();
	TopAbs_ShapeEnum t = (TopAbs_ShapeEnum) t_;

	if (unique) {
		TopTools_IndexedMapOfShape map;
		TopExp::MapShapes(s, t, map);
		return map.Extent();
	} else {
		int i = 0;
		TopExp_Explorer exp(s, t);
		for (; exp.More(); exp.Next()) {
			++i;
		}
		return i;
	}
}


int IfcGeom::Kernel::surface_genus(const ConversionResultShape* s_) {
	// @todo make kernel agnostic
	const TopoDS_Shape& s = ((OpenCascadeShape*) s_)->shape();
	OpenCascadeShape Ss(s);

	int nv = count(&Ss, (int) TopAbs_VERTEX, true);
	int ne = count(&Ss, (int) TopAbs_EDGE, true);
	int nf = count(&Ss, (int) TopAbs_FACE, true);

	const int euler = nv - ne + nf;
	const int genus = (2 - euler) / 2;

	return genus;
}

IfcGeom::impl::KernelFactoryImplementation& IfcGeom::impl::kernel_implementations() {
	static KernelFactoryImplementation impl;
	return impl;
}

extern void init_KernelImplementation_opencascade_Ifc2x3(IfcGeom::impl::KernelFactoryImplementation*);
extern void init_KernelImplementation_opencascade_Ifc4(IfcGeom::impl::KernelFactoryImplementation*);
#ifdef IFOPSH_USE_CGAL
extern void init_KernelImplementation_cgal_Ifc2x3(IfcGeom::impl::KernelFactoryImplementation*);
extern void init_KernelImplementation_cgal_Ifc4(IfcGeom::impl::KernelFactoryImplementation*);
#endif

IfcGeom::impl::KernelFactoryImplementation::KernelFactoryImplementation() {
	init_KernelImplementation_opencascade_Ifc2x3(this);
	init_KernelImplementation_opencascade_Ifc4(this);
#ifdef IFOPSH_USE_CGAL
	init_KernelImplementation_cgal_Ifc2x3(this);
	init_KernelImplementation_cgal_Ifc4(this);
#endif
}

void IfcGeom::impl::KernelFactoryImplementation::bind(const std::string& schema_name, const std::string& geometry_library, IfcGeom::impl::kernel_fn fn) {
	const std::string schema_name_lower = boost::to_lower_copy(schema_name);
	this->insert(std::make_pair(std::make_pair(schema_name_lower, geometry_library), fn));
}

IfcGeom::Kernel* IfcGeom::impl::KernelFactoryImplementation::construct(const std::string& schema_name, const std::string& geometry_library, IfcParse::IfcFile* file) {
	const std::string schema_name_lower = boost::to_lower_copy(schema_name);
	std::map<std::pair<std::string, std::string>, IfcGeom::impl::kernel_fn>::const_iterator it;
	it = this->find(std::make_pair(schema_name_lower, geometry_library));
	if (it == end()) {
		throw IfcParse::IfcException("No geometry kernel registered for " + schema_name);
	}
	return it->second(file);
}

#define CREATE_GET_DECOMPOSING_ENTITY(IfcSchema)                                                                 \
                                                                                                                 \
IfcSchema::IfcObjectDefinition* get_decomposing_entity_impl(IfcSchema::IfcProduct* product, bool include_openings) {\
	IfcSchema::IfcObjectDefinition* parent = 0;                                                                  \
                                                                                                                 \
	/* In case of an opening element, parent to the RelatingBuildingElement */                                   \
	if (include_openings && product->declaration().is(IfcSchema::IfcOpeningElement::Class())) {                  \
		IfcSchema::IfcOpeningElement* opening = (IfcSchema::IfcOpeningElement*)product;                          \
		IfcSchema::IfcRelVoidsElement::list::ptr voids = opening->VoidsElements();                               \
		if (voids->size()) {                                                                                     \
			IfcSchema::IfcRelVoidsElement* ifc_void = *voids->begin();                                           \
			parent = ifc_void->RelatingBuildingElement();                                                        \
		}                                                                                                        \
	} else if (product->declaration().is(IfcSchema::IfcElement::Class())) {                                      \
		IfcSchema::IfcElement* element = (IfcSchema::IfcElement*)product;                                        \
		IfcSchema::IfcRelFillsElement::list::ptr fills = element->FillsVoids();                                  \
		/* In case of a RelatedBuildingElement parent to the opening element */                                  \
		if (fills->size() && include_openings) {                                                                 \
			for (IfcSchema::IfcRelFillsElement::list::it it = fills->begin(); it != fills->end(); ++it) {        \
				IfcSchema::IfcRelFillsElement* fill = *it;                                                       \
				IfcSchema::IfcObjectDefinition* ifc_objectdef = fill->RelatingOpeningElement();                  \
				if (product == ifc_objectdef) continue;                                                          \
				parent = ifc_objectdef;                                                                          \
			}                                                                                                    \
		}                                                                                                        \
		/* Else simply parent to the containing structure */                                                     \
		if (!parent) {                                                                                           \
			IfcSchema::IfcRelContainedInSpatialStructure::list::ptr parents = element->ContainedInStructure();   \
			if (parents->size()) {                                                                               \
				IfcSchema::IfcRelContainedInSpatialStructure* container = *parents->begin();                     \
				parent = container->RelatingStructure();                                                         \
			}                                                                                                    \
		}                                                                                                        \
	}                                                                                                            \
                                                                                                                 \
	/* Parent decompositions to the RelatingObject */                                                            \
	if (!parent) {                                                                                               \
		IfcEntityList::ptr parents = product->data().getInverse((&IfcSchema::IfcRelAggregates::Class()), -1);    \
		parents->push(product->data().getInverse((&IfcSchema::IfcRelNests::Class()), -1));                       \
		for (IfcEntityList::it it = parents->begin(); it != parents->end(); ++it) {                              \
			IfcSchema::IfcRelDecomposes* decompose = (IfcSchema::IfcRelDecomposes*)*it;                          \
			IfcUtil::IfcBaseEntity* ifc_objectdef;                                                               \
                 																								 \
			ifc_objectdef = get_RelatingObject(decompose);                                                       \
                                                                                                                 \
			if (product == ifc_objectdef) continue;                                                              \
			parent = ifc_objectdef->as<IfcSchema::IfcObjectDefinition>();                                        \
		}                                                                                                        \
	}                                                                                                            \
	return parent;                                                                                               \
}

namespace {
	IfcUtil::IfcBaseEntity* get_RelatingObject(Ifc4::IfcRelDecomposes* decompose) {
		Ifc4::IfcRelAggregates* aggr = decompose->as<Ifc4::IfcRelAggregates>();
		if (aggr != nullptr) {
			return aggr->RelatingObject();
		}
		return nullptr;
	}

	IfcUtil::IfcBaseEntity* get_RelatingObject(Ifc2x3::IfcRelDecomposes* decompose) {
		return decompose->RelatingObject();
	}

	CREATE_GET_DECOMPOSING_ENTITY(Ifc2x3);
	CREATE_GET_DECOMPOSING_ENTITY(Ifc4);
}

IfcUtil::IfcBaseEntity* IfcGeom::Kernel::get_decomposing_entity(IfcUtil::IfcBaseEntity* inst, bool include_openings) {
	if (inst->as<Ifc2x3::IfcProduct>()) {
		return get_decomposing_entity_impl(inst->as<Ifc2x3::IfcProduct>(), include_openings);
	} else if (inst->as<Ifc4::IfcProduct>()) {
		return get_decomposing_entity_impl(inst->as<Ifc4::IfcProduct>(), include_openings);
	} else if (inst->declaration().name() == "IfcProject") {
		return nullptr;
	} else {
		throw IfcParse::IfcException("Unexpected entity " + inst->declaration().name());
	}
}

namespace {
	template <typename Schema>
	static std::map<std::string, IfcUtil::IfcBaseEntity*> get_layers_impl(typename Schema::IfcProduct* prod) {
		std::map<std::string, IfcUtil::IfcBaseEntity*> layers;
		if (prod->hasRepresentation()) {
			IfcEntityList::ptr r = IfcParse::traverse(prod->Representation());
			typename Schema::IfcRepresentation::list::ptr representations = r->template as<typename Schema::IfcRepresentation>();
			for (typename Schema::IfcRepresentation::list::it it = representations->begin(); it != representations->end(); ++it) {
				typename Schema::IfcPresentationLayerAssignment::list::ptr a = (*it)->LayerAssignments();
				for (typename Schema::IfcPresentationLayerAssignment::list::it jt = a->begin(); jt != a->end(); ++jt) {
					layers[(*jt)->Name()] = *jt;
				}
			}
		}
		return layers;
	}
}

std::map<std::string, IfcUtil::IfcBaseEntity*> IfcGeom::Kernel::get_layers(IfcUtil::IfcBaseEntity* inst) {
	if (inst->as<Ifc2x3::IfcProduct>()) {
		return get_layers_impl<Ifc2x3>(inst->as<Ifc2x3::IfcProduct>());
	} else if (inst->as<Ifc4::IfcProduct>()) {
		return get_layers_impl<Ifc4>(inst->as<Ifc4::IfcProduct>());
	} else {
		throw IfcParse::IfcException("Unexpected entity " + inst->declaration().name());
	}
}

bool IfcGeom::Kernel::is_manifold(const ConversionResultShape* s_) {
        // @todo make kernel agnostic
        const TopoDS_Shape& a = ((OpenCascadeShape*) s_)->shape();

	if (a.ShapeType() == TopAbs_COMPOUND || a.ShapeType() == TopAbs_SOLID) {
		TopoDS_Iterator it(a);
		for (; it.More(); it.Next()) {
			OpenCascadeShape s(it.Value());
			if (!is_manifold(&s)) {
				return false;
			}
		}
		return true;
	} else {
		TopTools_IndexedDataMapOfShapeListOfShape map;
		TopExp::MapShapesAndAncestors(a, TopAbs_EDGE, TopAbs_FACE, map);

		for (int i = 1; i <= map.Extent(); ++i) {
			if (map.FindFromIndex(i).Extent() != 2) {
				return false;
			}
		}

		return true;
	}
}

namespace {
	template <typename Schema>
	IfcEntityList::ptr find_openings_helper(typename Schema::IfcProduct* product) {

		typename IfcEntityList::ptr openings(new IfcEntityList);
		if (product->declaration().is(Schema::IfcElement::Class()) && !product->declaration().is(Schema::IfcOpeningElement::Class())) {
			typename Schema::IfcElement* element = (typename Schema::IfcElement*)product;
			openings = element->HasOpenings()->generalize();
		}

		// Is the IfcElement a decomposition of an IfcElement with any IfcOpeningElements?
		typename Schema::IfcObjectDefinition* obdef = product->template as<typename Schema::IfcObjectDefinition>();
		for (;;) {
			auto decomposes = obdef->Decomposes()->generalize();
			if (decomposes->size() != 1) break;
			typename Schema::IfcObjectDefinition* rel_obdef = (*decomposes->begin())->template as<typename Schema::IfcRelAggregates>()->RelatingObject();
			if (rel_obdef->declaration().is(Schema::IfcElement::Class()) && !rel_obdef->declaration().is(Schema::IfcOpeningElement::Class())) {
				typename Schema::IfcElement* element = (typename Schema::IfcElement*)rel_obdef;
				openings->push(element->HasOpenings()->generalize());
			}

			obdef = rel_obdef;
		}

		return openings;
	}
}

IfcEntityList::ptr IfcGeom::Kernel::find_openings(IfcUtil::IfcBaseEntity* inst) {
	if (inst->as<Ifc2x3::IfcProduct>()) {
		return find_openings_helper<Ifc2x3>(inst->as<Ifc2x3::IfcProduct>());
	} else if (inst->as<Ifc4::IfcProduct>()) {
		return find_openings_helper<Ifc4>(inst->as<Ifc4::IfcProduct>());
	} else {
		throw IfcParse::IfcException("Unexpected entity " + inst->declaration().name());
	}
}
