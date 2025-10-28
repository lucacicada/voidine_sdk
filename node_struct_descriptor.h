#pragma once

#include "core/math/basis.h"
#include "core/math/quaternion.h"
#include "core/string/string_name.h"
#include "core/templates/local_vector.h"
#include "core/variant/variant.h"
#include "scene/main/node.h"

class NodeStructDescriptor {
private:
	struct Member {
		NodePath name;

		bool operator==(const Member &p_to) { return name == p_to.name; }
		Member() {}
		Member(const NodePath &p_name) {
			name = p_name;
		}
	};

	TightLocalVector<Member> members;

public:
	uint32_t size() const {
		return members.size();
	}

	TypedArray<NodePath> get_properties() const {
		TypedArray<NodePath> props;
		for (uint32_t i = 0; i < members.size(); i++) {
			props.append(members[i].name);
		}
		return props;
	}

	void add_property(const NodePath &p_name) {
		members.push_back(Member(p_name));
	}

	int64_t property_get_index(const NodePath &p_name) const {
		return members.find(Member(p_name));
	}

	void remove_property(const NodePath &p_name) {
		members.erase(Member(p_name));
	}

	void encode_node(const Node &p_node, uint8_t *r_buffer, int &r_len) {
		if (!p_node) {
			r_len = 0;
			return;
		}

		uint8_t *buf = r_buffer;
		r_len = 0;

		if (!buf) {
			return;
		}

		uint8_t bool_byte = 0;
		int bool_bit = 0;

		for (uint32_t i = 0; i < members.size(); i++) {
			const Member &member = members[i];
			const StringName &property_name = member.name;

			const Variant &prop_value = p_node.get_indexed(property_name);

			switch (prop_value.get_type()) {
				case Variant::BOOL: {
					bool value = prop_value;
					if (value) {
						bool_byte |= (1 << bool_bit);
					}
					bool_bit++;
					if (bool_bit == 8) {
						*buf++ = bool_byte;
						r_len++;
						bool_byte = 0;
						bool_bit = 0;
					}
					break;
				}
				case Variant::INT: {
					r_len += (buf += encode_uint64((prop_value.operator int64_t()), buf));
					break;
				}
				case Variant::FLOAT: {
					r_len += (buf += encode_double((prop_value.operator double()), buf));
					break;
				}
				case Variant::VECTOR2I: {
					Vector2i v = prop_value;
					r_len += (buf += encode_uint32(v.x, buf));
					r_len += (buf += encode_uint32(v.y, buf));
					break;
				}
				case Variant::VECTOR3I: {
					Vector3i v = prop_value;
					r_len += (buf += encode_uint32(v.x, buf));
					r_len += (buf += encode_uint32(v.y, buf));
					r_len += (buf += encode_uint32(v.z, buf));
					break;
				}
				case Variant::VECTOR4I: {
					Vector4i v = prop_value;
					r_len += (buf += encode_uint32(v.x, buf));
					r_len += (buf += encode_uint32(v.y, buf));
					r_len += (buf += encode_uint32(v.z, buf));
					r_len += (buf += encode_uint32(v.w, buf));
					break;
				}
				case Variant::VECTOR2: {
					Vector2 v = prop_value;
					r_len += (buf += encode_real(v.x, buf));
					r_len += (buf += encode_real(v.y, buf));
					break;
				}
				case Variant::VECTOR3: {
					Vector3 v = prop_value;
					r_len += (buf += encode_real(v.x, buf));
					r_len += (buf += encode_real(v.y, buf));
					r_len += (buf += encode_real(v.z, buf));
					break;
				}
				case Variant::VECTOR4: {
					Vector4 v = prop_value;
					r_len += (buf += encode_real(v.x, buf));
					r_len += (buf += encode_real(v.y, buf));
					r_len += (buf += encode_real(v.z, buf));
					r_len += (buf += encode_real(v.w, buf));
					break;
				}
				case Variant::TRANSFORM2D: {
					Transform2D v = prop_value;
					r_len += (buf += encode_real(v.columns[0].x, buf));
					r_len += (buf += encode_real(v.columns[0].y, buf));
					r_len += (buf += encode_real(v.columns[1].x, buf));
					r_len += (buf += encode_real(v.columns[1].y, buf));
					r_len += (buf += encode_real(v.columns[2].x, buf));
					r_len += (buf += encode_real(v.columns[2].y, buf));
					break;
				}
				case Variant::TRANSFORM3D: {
					Transform3D v = prop_value;
					r_len += (buf += encode_real(v.basis.rows[0].x, buf));
					r_len += (buf += encode_real(v.basis.rows[0].y, buf));
					r_len += (buf += encode_real(v.basis.rows[0].z, buf));
					r_len += (buf += encode_real(v.basis.rows[1].x, buf));
					r_len += (buf += encode_real(v.basis.rows[1].y, buf));
					r_len += (buf += encode_real(v.basis.rows[1].z, buf));
					r_len += (buf += encode_real(v.basis.rows[2].x, buf));
					r_len += (buf += encode_real(v.basis.rows[2].y, buf));
					r_len += (buf += encode_real(v.basis.rows[2].z, buf));
					r_len += (buf += encode_real(v.origin.x, buf));
					r_len += (buf += encode_real(v.origin.y, buf));
					r_len += (buf += encode_real(v.origin.z, buf));
					break;
				}
				case Variant::BASIS: {
					Basis v = prop_value;
					r_len += (buf += encode_real(v.rows[0].x, buf));
					r_len += (buf += encode_real(v.rows[0].y, buf));
					r_len += (buf += encode_real(v.rows[0].z, buf));
					r_len += (buf += encode_real(v.rows[1].x, buf));
					r_len += (buf += encode_real(v.rows[1].y, buf));
					r_len += (buf += encode_real(v.rows[1].z, buf));
					r_len += (buf += encode_real(v.rows[2].x, buf));
					r_len += (buf += encode_real(v.rows[2].y, buf));
					r_len += (buf += encode_real(v.rows[2].z, buf));
					break;
				}
				case Variant::PROJECTION: {
					Projection v = prop_value;
					r_len += (buf += encode_real(v.columns[0].x, buf));
					r_len += (buf += encode_real(v.columns[0].y, buf));
					r_len += (buf += encode_real(v.columns[0].z, buf));
					r_len += (buf += encode_real(v.columns[0].w, buf));
					r_len += (buf += encode_real(v.columns[1].x, buf));
					r_len += (buf += encode_real(v.columns[1].y, buf));
					r_len += (buf += encode_real(v.columns[1].z, buf));
					r_len += (buf += encode_real(v.columns[1].w, buf));
					r_len += (buf += encode_real(v.columns[2].x, buf));
					r_len += (buf += encode_real(v.columns[2].y, buf));
					r_len += (buf += encode_real(v.columns[2].z, buf));
					r_len += (buf += encode_real(v.columns[2].w, buf));
					r_len += (buf += encode_real(v.columns[3].x, buf));
					r_len += (buf += encode_real(v.columns[3].y, buf));
					r_len += (buf += encode_real(v.columns[3].z, buf));
					r_len += (buf += encode_real(v.columns[3].w, buf));
					break;
				}
				case Variant::PLANE: {
					Plane v = prop_value;
					r_len += (buf += encode_real(v.normal.x, buf));
					r_len += (buf += encode_real(v.normal.y, buf));
					r_len += (buf += encode_real(v.normal.z, buf));
					r_len += (buf += encode_real(v.d, buf));
					break;
				}
				case Variant::QUATERNION: {
					Quaternion v = prop_value;
					r_len += (buf += encode_real(v.x, buf));
					r_len += (buf += encode_real(v.y, buf));
					r_len += (buf += encode_real(v.z, buf));
					r_len += (buf += encode_real(v.w, buf));
					break;
				}
				case Variant::RECT2: {
					Rect2 v = prop_value;
					r_len += (buf += encode_real(v.position.x, buf));
					r_len += (buf += encode_real(v.position.y, buf));
					r_len += (buf += encode_real(v.size.x, buf));
					r_len += (buf += encode_real(v.size.y, buf));
					break;
				}
				case Variant::RECT2I: {
					Rect2i v = prop_value;
					r_len += (buf += encode_uint32(v.position.x, buf));
					r_len += (buf += encode_uint32(v.position.y, buf));
					r_len += (buf += encode_uint32(v.size.x, buf));
					r_len += (buf += encode_uint32(v.size.y, buf));
					break;
				}
				case Variant::AABB: {
					AABB v = prop_value;
					r_len += (buf += encode_real(v.position.x, buf));
					r_len += (buf += encode_real(v.position.y, buf));
					r_len += (buf += encode_real(v.position.z, buf));
					r_len += (buf += encode_real(v.size.x, buf));
					r_len += (buf += encode_real(v.size.y, buf));
					r_len += (buf += encode_real(v.size.z, buf));
					break;
				}
				case Variant::COLOR: {
					Color v = prop_value; // color are RAW floats
					r_len += (buf += encode_real(v.r, buf));
					r_len += (buf += encode_real(v.g, buf));
					r_len += (buf += encode_real(v.b, buf));
					r_len += (buf += encode_real(v.a, buf));
					break;
				}
			}
		}

		if (bool_bit > 0) {
			*buf++ = bool_byte;
			r_len++;
		}
	}

	StructDescriptor() = default;
};
