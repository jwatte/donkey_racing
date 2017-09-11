from cafe2.proto import caffe2_pb2
import lmdb
import numpy as np

width = 149
height = 59

def create_db(output_file):
    print(">>> Write database...")
    LMDB_MAP_SIZE = 1 << 30   # MODIFY
    env = lmdb.open(output_file, map_size=LMDB_MAP_SIZE)

    checksum = 0
    with env.begin(write=True) as txn:
        for j in range(0, 16):
            # MODIFY: add your own data reader / creator

            img_data = np.random.rand(2, height, width)

            # Create TensorProtos
            tensor_protos = caffe2_pb2.TensorProtos()
            img_tensor = tensor_protos.protos.add()
            img_tensor.dims.extend(img_data.shape)
            img_tensor.data_type = 1

            flatten_img = img_data.reshape(np.prod(img_data.shape))
            img_tensor.float_data.extend(flatten_img.flat)

            label_tensor = tensor_protos.protos.add()
            label_tensor.data_type = 1
            label_tensor.float_data.append(0.5)
            label_tensor.float_data.append(-0.5)
            txn.put(
                    '{}'.format(j).encode('ascii'),
                    tensor_protos.SerializeToString()
                    )

            if (j % 16 == 0):
                print("Inserted {} rows".format(j))

create_db('some-lmdb')
