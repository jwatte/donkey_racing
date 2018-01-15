from __future__ import division, print_function
import numpy as np
import os
import sys
import time
import lmdb
import shutil
import traceback
import math
from caffe2.python import core, model_helper, net_drawer, workspace, visualize, brew
import caffe2.python.predictor.predictor_exporter as pe
import caffe2.python.predictor.predictor_py_utils as pred_utils
from caffe2.python.predictor_constants import predictor_constants as pc
from caffe2.proto import caffe2_pb2

BATCH_SIZE=64
# for base training
#LEARN_RATE=-0.005
#train_iters=500000
# for re-training
LEARN_RATE=-0.001
train_iters=50000
iter_val=0

#root_folder = "/home/jwatte/trainingdata/donkey_racing/network"
root_folder = "/usr/local/src/trainingdata"
#data_folder = "/home/jwatte/trainingdata/2017-09-07-8am-park"
data_folder = os.path.join(root_folder, 'temp_oakland')

def AddInput(model, batch_size, db, db_type):
    data_uint8, label = model.TensorProtosDBInput(
        [], ["data_uint8", "label"], batch_size=batch_size,
        db=db, db_type=db_type)
    data = model.Cast(data_uint8, "data_cast", to=core.DataType.FLOAT)
    data2 = model.Scale(data, 'data2', scale=float(1./255))
    data3 = model.StopGradient(data2, 'data3')
    return data3, label

# This is a LeNet-like model
#
def AddNetModel_2(model, data):
    # 182x70x1 -> 90x34x3
    conv1 = brew.conv(model, data, 'conv1', dim_in=1, dim_out=3, kernel=3)
    pool1 = brew.max_pool(model, conv1, 'pool1', kernel=2, stride=2)
    # 90x34x3 -> 44x16x5
    conv2 = brew.conv(model, pool1, 'conv2', dim_in=3, dim_out=5, kernel=3)
    pool2 = brew.max_pool(model, conv2, 'pool2', kernel=2, stride=2)
    # 44x16x5 -> 21x7x7
    conv3 = brew.conv(model, pool2, 'conv3', dim_in=5, dim_out=7, kernel=3)
    pool3 = brew.max_pool(model, conv3, 'pool3', kernel=2, stride=2)
    relu4 = brew.relu(model, pool3, 'relu4')
    # 21x7x7 -> 2
    fc5 = brew.fc(model, relu4, 'fc5', dim_in=21*7*7, dim_out=16)
    relu5 = brew.relu(model, fc5, 'relu5')
    output = brew.fc(model, relu5, 'output', dim_in=16, dim_out=2)
    return output

def AddNetModel_3(model, data):
    # 182x70x1 -> 90x34x3
    conv1 = brew.conv(model, data, 'conv1', dim_in=1, dim_out=6, kernel=3)
    pool1 = brew.max_pool(model, conv1, 'pool1', kernel=2, stride=2)
    # 90x34x3 -> 44x16x5
    conv2 = brew.conv(model, pool1, 'conv2', dim_in=6, dim_out=8, kernel=3)
    pool2 = brew.max_pool(model, conv2, 'pool2', kernel=2, stride=2)
    # 44x16x5 -> 21x7x7
    conv3 = brew.conv(model, pool2, 'conv3', dim_in=8, dim_out=10, kernel=3)
    pool3 = brew.max_pool(model, conv3, 'pool3', kernel=2, stride=2)
    relu4 = brew.relu(model, pool3, 'relu4')
    # 21x7x7 -> 2
    fc5 = brew.fc(model, relu4, 'fc5', dim_in=21*7*10, dim_out=32)
    relu5 = brew.relu(model, fc5, 'relu5')
    output = brew.fc(model, relu5, 'output', dim_in=32, dim_out=2)
    return output

def AddNetModel_5(model, data):
    # 0 params, dims [1, 70, 182]
    input1 = data
    # 18 params, dims [2, 68, 180]
    conv2 = brew.conv(model, input1, 'conv2', dim_in=1, dim_out=2, kernel=3, stride=1)
    # 0 params, dims [2, 34, 90]
    pool3 = brew.max_pool(model, conv2, 'pool3', kernel=2, stride=2)
    # 72 params, dims [4, 32, 88]
    conv4 = brew.conv(model, pool3, 'conv4', dim_in=2, dim_out=4, kernel=3, stride=1)
    # 0 params, dims [4, 16, 44]
    pool5 = brew.max_pool(model, conv4, 'pool5', kernel=2, stride=2)
    # 288 params, dims [8, 14, 42]
    conv6 = brew.conv(model, pool5, 'conv6', dim_in=4, dim_out=8, kernel=3, stride=1)
    # 0 params, dims [8, 14, 42]
    relu7 = brew.relu(model, conv6, 'relu7')
    # 64 params, dims [2, 7, 21]
    conv8 = brew.conv(model, relu7, 'conv8', dim_in=8, dim_out=2, kernel=2, stride=2)
    # 0 params, dims [2, 7, 21]
    relu9 = brew.relu(model, conv8, 'relu9')
    # 784 params, dims [8, 1, 3]
    conv10 = brew.conv(model, relu9, 'conv10', dim_in=2, dim_out=8, kernel=7, stride=7)
    # 0 params, dims [8, 1, 3]
    relu11 = brew.relu(model, conv10, 'relu11')
    # 192 params, dims [8]
    fc12 = brew.fc(model, relu11, 'fc12', dim_in=24, dim_out=8)
    # 0 params, dims [8]
    relu13 = brew.relu(model, fc12, 'relu13')
    # 16 params, dims [2]
    output = brew.fc(model, relu13, 'output', dim_in=8, dim_out=2)
    return output

def AddNetModel_6(model, data):
    # 0 params, dims [1, 146, 146]
    input1 = data
    # 72 params, dims [8, 144, 144]
    conv2 = brew.conv(model, input1, 'conv2', dim_in=1, dim_out=8, kernel=3, stride=1)
    # 0 params, dims [8, 144, 144]
    relu3 = brew.relu(model, conv2, 'relu3')
    # 128 params, dims [4, 72, 72]
    conv4 = brew.conv(model, relu3, 'conv4', dim_in=8, dim_out=4, kernel=2, stride=2)
    # 1600 params, dims [16, 68, 68]
    conv5 = brew.conv(model, conv4, 'conv5', dim_in=4, dim_out=16, kernel=5, stride=1)
    # 0 params, dims [16, 68, 68]
    relu6 = brew.relu(model, conv5, 'relu6')
    # 512 params, dims [8, 34, 34]
    conv7 = brew.conv(model, relu6, 'conv7', dim_in=16, dim_out=8, kernel=2, stride=2)
    # 2304 params, dims [32, 32, 32]
    conv8 = brew.conv(model, conv7, 'conv8', dim_in=8, dim_out=32, kernel=3, stride=1)
    # 0 params, dims [32, 32, 32]
    relu9 = brew.relu(model, conv8, 'relu9')
    # 2048 params, dims [16, 16, 16]
    conv10 = brew.conv(model, relu9, 'conv10', dim_in=32, dim_out=16, kernel=2, stride=2)
    # 9216 params, dims [64, 14, 14]
    conv11 = brew.conv(model, conv10, 'conv11', dim_in=16, dim_out=64, kernel=3, stride=1)
    # 0 params, dims [64, 14, 14]
    relu12 = brew.relu(model, conv11, 'relu12')
    # 8192 params, dims [32, 7, 7]
    conv13 = brew.conv(model, relu12, 'conv13', dim_in=64, dim_out=32, kernel=2, stride=2)
    # 0 params, dims [32, 7, 7]
    relu14 = brew.relu(model, conv13, 'relu14')
    # 301056 params, dims [192]
    fc15 = brew.fc(model, relu14, 'fc15', dim_in=1568, dim_out=192)
    # 0 params, dims [192]
    relu16 = brew.relu(model, fc15, 'relu16')
    # 384 params, dims [2]
    output = brew.fc(model, relu16, 'output', dim_in=192, dim_out=2)
    # 325512 parameters total
    return output

def AddNetModel_8(model, data):
    # 0 params, dims [3, 62, 150], ops 27900
    input1 = data
    # 216 params, dims [8, 60, 148], ops 1918080
    conv2 = brew.conv(model, input1, 'conv2', dim_in=3, dim_out=8, kernel=3, stride=1)
    # 0 params, dims [8, 30, 74], ops 17760
    pool3 = brew.max_pool(model, conv2, 'pool3', kernel=2, stride=2)
    # 1152 params, dims [16, 28, 72], ops 2322432
    conv4 = brew.conv(model, pool3, 'conv4', dim_in=8, dim_out=16, kernel=3, stride=1)
    # 0 params, dims [16, 28, 72], ops 32256
    relu5 = brew.relu(model, conv4, 'relu5')
    # 128 params, dims [8, 28, 72], ops 258048
    conv6 = brew.conv(model, relu5, 'conv6', dim_in=16, dim_out=8, kernel=1, stride=1)
    # 0 params, dims [8, 14, 36], ops 4032
    pool7 = brew.max_pool(model, conv6, 'pool7', kernel=2, stride=2)
    # 2304 params, dims [32, 12, 34], ops 940032
    conv8 = brew.conv(model, pool7, 'conv8', dim_in=8, dim_out=32, kernel=3, stride=1)
    # 0 params, dims [32, 12, 34], ops 13056
    relu9 = brew.relu(model, conv8, 'relu9')
    # 256 params, dims [8, 12, 34], ops 104448
    conv10 = brew.conv(model, relu9, 'conv10', dim_in=32, dim_out=8, kernel=1, stride=1)
    # 0 params, dims [8, 6, 17], ops 816
    pool11 = brew.max_pool(model, conv10, 'pool11', kernel=2, stride=2)
    # 4608 params, dims [64, 4, 15], ops 276480
    conv12 = brew.conv(model, pool11, 'conv12', dim_in=8, dim_out=64, kernel=3, stride=1)
    # 0 params, dims [64, 4, 15], ops 3840
    relu13 = brew.relu(model, conv12, 'relu13')
    # 2048 params, dims [32, 4, 15], ops 122880
    conv14 = brew.conv(model, relu13, 'conv14', dim_in=64, dim_out=32, kernel=1, stride=1)
    # 0 params, dims [32, 4, 15], ops 1920
    relu15 = brew.relu(model, conv14, 'relu15')
    # 245760 params, dims [128], ops 31457280
    fc16 = brew.fc(model, relu15, 'fc16', dim_in=1920, dim_out=128)
    # 0 params, dims [128], ops 128
    relu17 = brew.relu(model, fc16, 'relu17')
    # 256 params, dims [2], ops 512
    output = brew.fc(model, relu17, 'output', dim_in=128, dim_out=2)
    # 256728 parameters total
    # 37501900 operations total
    return output


def AddNetModel(model, data):
    return AddNetModel_8(model, data)

def AddTrainingOperators(model, output, label):
    loss = model.SquaredL2Distance([output, label], "loss")
    avgloss = model.AveragedLoss([loss], "avgloss")
    model.AddGradientOperators([avgloss])
    ITER = brew.iter(model, "iter")
    stepsize = int(train_iters / 2000)
    if (stepsize > 30):
        stepsize = int(30 + (stepsize - 30) / 5)
    if stepsize < 1:
        stepsize = 1
    assert(LEARN_RATE < 0)
    LR = model.LearningRate(
        ITER, "LR", base_lr=LEARN_RATE, policy="step", stepsize=stepsize, gamma=0.9995)
    ONE = model.param_init_net.ConstantFill([], "ONE", shape=[1], value=1.0)
    for param in model.params:
        param_grad = model.param_to_grad[param]
        model.WeightedSum([param, ONE, param_grad, LR], param)
    model.Checkpoint([ITER] + model.params, [],
        db="checkpoint_%06d.lmdb", db_type="lmdb", every=10000)

def AddBookkeepingOperators(model):
    model.Print('loss', [], to_file=1)
    for param in model.params:
        model.Summarize(param, [], to_file=1)
        model.Summarize(model.param_to_grad[param], [], to_file=1)

def build_networks():
    arg_scope = {"order": "NCHW", "use_cudnn":True}
    train_model = model_helper.ModelHelper(name="donkey_train", arg_scope=arg_scope)
    dbpath = os.path.join(data_folder, 'train')
    # print('dbpath = %s' % (dbpath,))
    data, label = AddInput(
        train_model, batch_size=BATCH_SIZE,
        db=dbpath,
        db_type='lmdb')
    output = AddNetModel(train_model, data)
    AddTrainingOperators(train_model, output, label)
    AddBookkeepingOperators(train_model)

    test_model = model_helper.ModelHelper(
        name="donkey_test", arg_scope=arg_scope, init_params=False)
    data, label = AddInput(
        test_model, batch_size=1,
        db=os.path.join(data_folder, 'test'),
        db_type='lmdb')
    output = AddNetModel(test_model, data)

    deploy_model = model_helper.ModelHelper(
        name="donkey_deploy", arg_scope=arg_scope, init_params=False)
    AddNetModel(deploy_model, "input")

    return (train_model, test_model, deploy_model)

def write_block(fil, nam, info, blob, bsize):
    fil.write("%s\n" % (nam,))
    fil.write("%s\n" % (info,))
    fil.write("%d\n" % (bsize,))
    fil.write(blob)
    fil.write("\n-\n")

def save_trained_model(deploy_model):
    savepath = os.path.join(root_folder, "donkey_model_protos.crunk")
    print('savepath = %s' % (savepath,))
    try:
        shutil.rmtree(savepath)
    except:
        pass
    with open(savepath, 'wb') as of:
        of.write("crunk 1.0\n")
        ss = deploy_model.net.Proto().SerializeToString()
        write_block(of, ':NET', '', ss, len(ss))
        parameters = [str(b) for b in deploy_model.params]
        for i in parameters:
            b = workspace.FetchBlob(i)
            print(i + ": " + repr(b.shape))
            write_block(of, i, repr(b.shape), b, b.size * b.itemsize)

    print("The deploy model is saved to: " + savepath)


def load_crunk(name, device_opts=None):
    with open(name, 'rb') as rdf:
        l = rdf.readline()
        if l <> 'crunk 1.0\n':
            raise RuntimeError(name + ': not a crunk 1.0 file')
        while True:
            name = rdf.readline()
            if name == '':
                break
            name = name.strip()
            info = rdf.readline()
            info = info.strip()
            shape = None
            if info <> '':
                shape = eval(info.strip())
            size = rdf.readline()
            size = int(size.strip())
            block = rdf.read(size)
            if shape:
                # print("# ", name, shape, size, len(block))
                block = np.fromstring(block, dtype=np.float32)
                block = np.reshape(block, shape)
                workspace.FeedBlob(name, block, device_opts)
            empty = rdf.readline()
            dash = rdf.readline()
            if dash.strip() <> '-':
                raise RuntimeError(name + ': mis-formatted crunk 1.0 file (got ' + dash + ' expected "-")')

